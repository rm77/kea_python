#include "keamodule.h"
#include <dhcpsrv/lease_mgr_factory.h>

using namespace std;
using namespace isc::dhcp;
using namespace isc::asiolink;

extern "C" {

static PyObject *
LeaseMgr_getLease4(LeaseMgrObject *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"addr", "hwaddr", "client_id", "subnet_id", NULL};
    char *addr = 0;
    char *hwaddr = 0;
    char *client_id = 0;
    unsigned long subnet_id = 0;
    bool have_subnet_id = false;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sssk", (char **)kwlist, &addr, &hwaddr, &client_id, &subnet_id)) {
        return (0);
    }
    PyObject *tmp = PyDict_GetItemString(kwargs, "subnet_id");
    if (tmp) {
        have_subnet_id = true;
        Py_DECREF(tmp);
    }

    try {
        // Lease4Ptr getLease4(const IOAddress &addr)
        // Lease4Ptr getLease4(const HWAddr &hwaddr, SubnetID subnet_id)
        // Lease4Ptr getLease4(const ClientId &clientid, SubnetID subnet_id)
        // Lease4Ptr getLease4(const ClientId &client_id, const HWAddr &hwaddr, SubnetID subnet_id)
        // TODO
        // Lease4Collection getLease4(const ClientId &clientid)
        // Lease4Collection getLease4(const HWAddr &hwaddr)
        Lease4Ptr ptr;
        if (addr != 0 && !hwaddr && !client_id && !have_subnet_id) {
            ptr = self->mgr->getLease4(IOAddress(string(addr)));
        }
        else if (!addr && hwaddr != 0 && !client_id && have_subnet_id) {
            HWAddr hw = HWAddr::fromText(hwaddr);
            ptr = self->mgr->getLease4(hw, subnet_id);
        }
        else if (!addr && !hwaddr && client_id != 0 && have_subnet_id) {
            ClientIdPtr clientid_ptr = ClientId::fromText(client_id);
            ptr = self->mgr->getLease4(*clientid_ptr, subnet_id);
        }
        else if (!addr && hwaddr != 0 && client_id != 0 && have_subnet_id) {
            ClientIdPtr clientid_ptr = ClientId::fromText(client_id);
            HWAddr hw = HWAddr::fromText(hwaddr);
            ptr = self->mgr->getLease4(*clientid_ptr, hw, subnet_id);
        }
        else {
            PyErr_SetString(PyExc_TypeError, "Invalid argument combination");
            return (0);
        }
        if (!ptr) {
            Py_RETURN_NONE;
        }
        return (Lease4_from_handle(ptr));
    }
    catch (const exception &e) {
        PyErr_SetString(PyExc_TypeError, e.what());
        return (0);
    }
}

static PyObject *
LeaseMgr_getLeases4(LeaseMgrObject *self, PyObject *args, PyObject *kwargs) {
#if HAVE_GETLEASES4_HOSTNAME
    #define KWLIST {"subnet_id", "hostname", "lower_bound_address", "page_size", NULL}
    #define KWFORMAT "|kssk"
    #define KWVARS &subnet_id, &hostname, &lower_bound_address, &page_size
#else
    #define KWLIST {"subnet_id", "lower_bound_address", "page_size", NULL}
    #define KWFORMAT "|ksk"
    #define KWVARS &subnet_id, &lower_bound_address, &page_size
#endif
    static const char *kwlist[] = KWLIST;
    unsigned long subnet_id = 0;
    char *hostname = 0;
    char *lower_bound_address = 0;
    unsigned long page_size = 0;
    bool have_subnet_id = false;
    bool have_page_size = false;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, KWFORMAT, (char **)kwlist, KWVARS)) {
        return (0);
    }
    PyObject *tmp = PyDict_GetItemString(kwargs, "subnet_id");
    if (tmp) {
        have_subnet_id = true;
        Py_DECREF(tmp);
    }
    tmp = PyDict_GetItemString(kwargs, "page_size");
    if (tmp) {
        have_page_size = true;
        Py_DECREF(tmp);
    }

    PyObject *list = PyList_New(0);
    if (!list) {
        return (0);
    }
    try {
        // Lease4Collection getLeases4(SubnetID subnet_id)
        // Lease4Collection getLeases4(const std::string &hostname)
        // Lease4Collection getLeases4()
        // Lease4Collection getLeases4(const IOAddress &lower_bound_address, const LeasePageSize &page_size)
        Lease4Collection leases;
        if (have_subnet_id && !hostname && !lower_bound_address && !have_page_size) {
            leases = self->mgr->getLeases4((SubnetID) subnet_id);
        }
#if HAVE_GETLEASES4_HOSTNAME
        else if (!have_subnet_id && hostname != 0 && !lower_bound_address && !have_page_size) {
            leases = self->mgr->getLeases4(string(hostname));
        }
#endif
        else if (!have_subnet_id && !hostname && !lower_bound_address && !have_page_size) {
            leases = self->mgr->getLeases4();
        }
        else if (!have_subnet_id && !hostname && lower_bound_address != 0 && have_page_size) {
            leases = self->mgr->getLeases4(IOAddress(string(lower_bound_address)), LeasePageSize(page_size));
        }
        else {
            PyErr_SetString(PyExc_TypeError, "Invalid argument combination");
            Py_DECREF(list);
            return (0);
        }
        for (auto lease : leases) {
            PyObject *obj = Lease4_from_handle(lease);
            if (!obj || PyList_Append(list, obj) < 0) {
                Py_DECREF(list);
                return (0);
            }
        }
    }
    catch (const exception &e) {
        PyErr_SetString(PyExc_TypeError, e.what());
        Py_DECREF(list);
        return (0);
    }

    return (list);
}

static PyMethodDef LeaseMgr_methods[] = {
    {"getLease4", (PyCFunction) LeaseMgr_getLease4, METH_VARARGS | METH_KEYWORDS,
     "Returns an IPv4 lease for specified IPv4 address."},
    {"getLeases4", (PyCFunction) LeaseMgr_getLeases4, METH_VARARGS | METH_KEYWORDS,
     "Returns all IPv4 leases for the particular subnet identifier."},
    {0}  // Sentinel
};

static void
LeaseMgr_dealloc(LeaseMgrObject *self) {
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
LeaseMgr_init(LeaseMgrObject *self, PyObject *args, PyObject *kwds) {
    if (kwds != 0) {
        PyErr_SetString(PyExc_TypeError, "keyword arguments are not supported");
        return (0);
    }
    if (!PyArg_ParseTuple(args, "")) {
        return (-1);
    }

    try {
        self->mgr = &LeaseMgrFactory::instance();
    }
    catch (const exception &e) {
        PyErr_SetString(PyExc_TypeError, e.what());
        return (-1);
    }

    return (0);
}

static PyObject *
LeaseMgr_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    LeaseMgrObject *self;
    self = (LeaseMgrObject *) type->tp_alloc(type, 0);
    if (self) {
        self->mgr = 0;
    }
    return ((PyObject *) self);
}

PyTypeObject LeaseMgrType = {
    PyObject_HEAD_INIT(0)
    "kea.LeaseMgr",                             // tp_name
    sizeof(LeaseMgrObject),                     // tp_basicsize
    0,                                          // tp_itemsize
    (destructor) LeaseMgr_dealloc,              // tp_dealloc
    0,                                          // tp_vectorcall_offset
    0,                                          // tp_getattr
    0,                                          // tp_setattr
    0,                                          // tp_as_async
    0,                                          // tp_repr
    0,                                          // tp_as_number
    0,                                          // tp_as_sequence
    0,                                          // tp_as_mapping
    0,                                          // tp_hash
    0,                                          // tp_call
    0,                                          // tp_str
    0,                                          // tp_getattro
    0,                                          // tp_setattro
    0,                                          // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                         // tp_flags
    "Kea server LeaseMgr",                      // tp_doc
    0,                                          // tp_traverse
    0,                                          // tp_clear
    0,                                          // tp_richcompare
    0,                                          // tp_weaklistoffset
    0,                                          // tp_iter
    0,                                          // tp_iternext
    LeaseMgr_methods,                           // tp_methods
    0,                                          // tp_members
    0,                                          // tp_getset
    0,                                          // tp_base
    0,                                          // tp_dict
    0,                                          // tp_descr_get
    0,                                          // tp_descr_set
    0,                                          // tp_dictoffset
    (initproc) LeaseMgr_init,                   // tp_init
    PyType_GenericAlloc,                        // tp_alloc
    LeaseMgr_new                                // tp_new
};

int
LeaseMgr_define() {
    if (PyType_Ready(&LeaseMgrType) < 0) {
        return (1);
    }
    Py_INCREF(&LeaseMgrType);
    if (PyModule_AddObject(kea_module, "LeaseMgr", (PyObject *) &LeaseMgrType) < 0) {
        Py_DECREF(&LeaseMgrType);
        return (1);
    }

    return (0);
}

}