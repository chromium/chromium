// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::pin::Pin;

use crate::rs_glue;

/// A reference to a C++ `base::Value` of type `base::Value::Type::DICTIONARY`.
/// Such a value is currently either held directly in a populated
/// [`ValueSlotRef`] or in a child `base::Value` thereof.
pub struct DictValueRef<'a>(Pin<&'a mut rs_glue::ffi::Value>);

impl<'a> DictValueRef<'a> {
    /// Get a reference to the base::Value.
    fn raw_mut(&mut self) -> Pin<&mut rs_glue::ffi::Value> {
        self.0.as_mut()
    }

    /// Sets the value at this dictionary key to be a value of type
    /// `base::Value::Type::NONE`.
    pub fn set_none_key(&mut self, key: &str) {
        rs_glue::ffi::ValueSetNoneKey(self.raw_mut(), key);
    }

    /// Sets the value at this dictionary key to a Boolean.
    pub fn set_bool_key(&mut self, key: &str, val: bool) {
        rs_glue::ffi::ValueSetBoolKey(self.raw_mut(), key, val);
    }

    /// Sets the value at this dictionary key to an integer.
    pub fn set_integer_key(&mut self, key: &str, val: i32) {
        rs_glue::ffi::ValueSetIntegerKey(self.raw_mut(), key, val);
    }

    /// Sets the value at this dictionary key to a double.
    pub fn set_double_key(&mut self, key: &str, val: f64) {
        rs_glue::ffi::ValueSetDoubleKey(self.raw_mut(), key, val);
    }

    /// Sets the value at this dictionary key to a string.
    pub fn set_string_key(&mut self, key: &str, val: &str) {
        rs_glue::ffi::ValueSetStringKey(self.raw_mut(), key, val);
    }

    /// Sets the value at this dictionary key to a new dictionary, and returns
    /// a reference to it.
    pub fn set_dict_key(&mut self, key: &str) -> DictValueRef {
        rs_glue::ffi::ValueSetDictKey(self.raw_mut(), key).into()
    }

    /// Sets the value at this dictionary key to a new list, and returns a
    /// reference to it.
    pub fn set_list_key(&mut self, key: &str) -> ListValueRef {
        rs_glue::ffi::ValueSetListKey(self.raw_mut(), key).into()
    }
}

impl<'a> From<Pin<&'a mut rs_glue::ffi::Value>> for DictValueRef<'a> {
    /// Wrap a reference to a C++ `base::Value` in a newtype wrapper to
    /// indicate that it's of type DICTIONARY. This is not actually unsafe,
    /// since any mistakes here will result in a deliberate crash due to
    /// assertions on the C++ side, rather than memory safety errors.
    fn from(value: Pin<&'a mut rs_glue::ffi::Value>) -> Self {
        Self(value)
    }
}

/// A reference to a C++ `base::Value` of type `base::Value::Type::LIST`.
/// Such a value is currently either held directly in a populated
/// [`ValueSlotRef`] or in a child `base::Value` thereof.
pub struct ListValueRef<'a>(Pin<&'a mut rs_glue::ffi::Value>);

impl<'a> ListValueRef<'a> {
    /// Get a reference to the underlying base::Value.
    fn raw_mut(&mut self) -> Pin<&mut rs_glue::ffi::Value> {
        self.0.as_mut()
    }

    /// Appends a value of type `base::Value::Type::NONE`. Grows
    /// the list as necessary.
    pub fn append_none(&mut self) {
        rs_glue::ffi::ValueAppendNone(self.raw_mut());
    }

    /// Appends a Boolean. Grows the list as necessary.
    pub fn append_bool(&mut self, val: bool) {
        self.raw_mut().ValueAppendBool(val)
    }

    /// Appends an integer. Grows the list as necessary.
    pub fn append_integer(&mut self, val: i32) {
        self.raw_mut().ValueAppendInteger(val)
    }

    /// Appends a double. Grows the list as necessary.
    pub fn append_double(&mut self, val: f64) {
        self.raw_mut().ValueAppendDouble(val)
    }

    /// Appends a string. Grows the list as necessary.
    pub fn append_string(&mut self, val: &str) {
        rs_glue::ffi::ValueAppendString(self.raw_mut(), val);
    }

    /// Appends a new dictionary, and returns a reference to it.
    /// Grows the list as necessary.
    pub fn append_dict(&mut self) -> DictValueRef {
        rs_glue::ffi::ValueAppendDict(self.raw_mut()).into()
    }

    /// Appends a new list, and returns a reference to it. Grows
    /// the list as necessary.
    pub fn append_list(&mut self) -> ListValueRef {
        rs_glue::ffi::ValueAppendList(self.raw_mut()).into()
    }

    /// Reserves space for a given number of elements within a list. This is
    /// optional - lists will grow as necessary to accommodate the items you
    /// add, so this just reduces the allocations necessary.
    pub fn reserve_size(&mut self, len: usize) {
        rs_glue::ffi::ValueReserveSize(self.raw_mut(), len);
    }
}

impl<'a> From<Pin<&'a mut rs_glue::ffi::Value>> for ListValueRef<'a> {
    /// Wrap a reference to a C++ `base::Value` in a newtype wrapper to
    /// indicate that it's of type LIST. This is not actually unsafe, since
    /// any mistakes here will result in a deliberate crash due to assertions
    /// on the C++ side, rather than memory safety errors.
    fn from(value: Pin<&'a mut rs_glue::ffi::Value>) -> Self {
        Self(value)
    }
}

/// A reference to a slot in which a `base::Value` can be constructed.
/// Such a slot can only be created within C++ and passed to Rust; Rust
/// can then create a `base::Value` therein.
pub struct ValueSlotRef<'a>(Pin<&'a mut rs_glue::ffi::ValueSlot>);

impl<'a> From<Pin<&'a mut rs_glue::ffi::ValueSlot>> for ValueSlotRef<'a> {
    fn from(value: Pin<&'a mut rs_glue::ffi::ValueSlot>) -> Self {
        Self(value)
    }
}

impl<'a> From<&'a mut cxx::UniquePtr<rs_glue::ffi::ValueSlot>> for ValueSlotRef<'a> {
    fn from(value: &'a mut cxx::UniquePtr<rs_glue::ffi::ValueSlot>) -> Self {
        Self(value.pin_mut())
    }
}

impl<'a> ValueSlotRef<'a> {
    /// Return a mutable reference to the underlying raw value.
    fn raw_mut(&mut self) -> Pin<&mut rs_glue::ffi::ValueSlot> {
        self.0.as_mut()
    }

    /// Return a reference to the underlying raw value.
    fn raw(&self) -> &rs_glue::ffi::ValueSlot {
        &self.0
    }

    /// Creates a new `base::Value::Type::NONE` `base::Value` in this slot.
    pub fn construct_none(&mut self) {
        rs_glue::ffi::ConstructNoneValue(self.raw_mut());
    }

    /// Creates a new Boolean `base::Value` in this slot.
    pub fn construct_bool(&mut self, val: bool) {
        rs_glue::ffi::ConstructBoolValue(self.raw_mut(), val);
    }

    /// Creates a new integer `base::Value` in this slot.
    pub fn construct_integer(&mut self, val: i32) {
        rs_glue::ffi::ConstructIntegerValue(self.raw_mut(), val);
    }

    /// Creates a new double `base::Value` in this slot.
    pub fn construct_double(&mut self, val: f64) {
        rs_glue::ffi::ConstructDoubleValue(self.raw_mut(), val);
    }

    /// Creates a new string `base::Value` in this slot.
    pub fn construct_string(&mut self, val: &str) {
        rs_glue::ffi::ConstructStringValue(self.raw_mut(), val);
    }

    /// Creates a new dictionary `base::Value` in this slot.
    pub fn construct_dict(&mut self) -> DictValueRef {
        rs_glue::ffi::ConstructDictValue(self.raw_mut()).into()
    }

    /// Creates a new list `base::Value` in this slot.
    pub fn construct_list(&mut self) -> ListValueRef {
        rs_glue::ffi::ConstructListValue(self.raw_mut()).into()
    }
}

/// Asks C++ code to dump this base::Value back to JSON.
/// Primarily for testing the round-trip.
impl<'a> std::fmt::Debug for ValueSlotRef<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        f.write_str(&rs_glue::ffi::DumpValueSlot(self.raw()))
    }
}
