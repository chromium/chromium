// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// C++ bindings for base.
///
/// This mod contains all the FFI bindings for C++ types in the base
/// namespace. It should declare shared types, and the 'extern "C++"'
/// section indicating C++ functions that are callable from Rust.
///
/// Other [cxx::bridge] mods throughout the //base codebase may exist
/// for exporting Rust functions to C++.
///
/// C++ functions which exist only for the benefit of calls from
/// Rust->C++ should live within the base::rs_glue C++ namespace.
#[cxx::bridge(namespace=base::rs_glue)]
pub(crate) mod ffi {
    unsafe extern "C++" {
        include!("base/rs_glue/values_glue.h");
        /// C++ `base::Value`
        #[namespace=base]
        type Value;

        // Bindings to existing base::Value methods which happen to
        // line up with our needs precisely.
        #[cxx_name = "Append"]
        fn ValueAppendBool(self: Pin<&mut Value>, val: bool);
        #[cxx_name = "Append"]
        fn ValueAppendInteger(self: Pin<&mut Value>, val: i32);
        #[cxx_name = "Append"]
        fn ValueAppendDouble(self: Pin<&mut Value>, val: f64);

        // Free functions in C++ for cases where existing base::Value
        // APIs don't quite match our needs.

        // Set a key on a base::Value of type DICTIONARY to the given
        // value.
        fn ValueSetNoneKey(v: Pin<&mut Value>, key: &str);
        fn ValueSetBoolKey(v: Pin<&mut Value>, key: &str, val: bool);
        fn ValueSetIntegerKey(v: Pin<&mut Value>, key: &str, val: i32);
        fn ValueSetDoubleKey(v: Pin<&mut Value>, key: &str, val: f64);
        fn ValueSetStringKey(v: Pin<&mut Value>, key: &str, value: &str);
        // Returns a new child base::Value, of type DICTIONARY.
        fn ValueSetDictKey<'a>(v: Pin<&'a mut Value>, key: &str) -> Pin<&'a mut Value>;
        // Returns a new child base::Value, of type LIST.
        fn ValueSetListKey<'a>(v: Pin<&'a mut Value>, key: &str) -> Pin<&'a mut Value>;

        // Appends to a base::Value of type LIST.
        fn ValueAppendNone(v: Pin<&mut Value>);
        fn ValueAppendString(v: Pin<&mut Value>, value: &str);
        // Returns a new child base::Value, of type DICTIONARY.
        fn ValueAppendDict(v: Pin<&mut Value>) -> Pin<&mut Value>;
        // Returns a new child base::Value, of type LIST.
        fn ValueAppendList(v: Pin<&mut Value>) -> Pin<&mut Value>;
        fn ValueReserveSize(v: Pin<&mut Value>, len: usize);

        /// Represents a slot (on stack or heap) into which a new
        /// `base::Value` can be constructed. Only C++ code can construct
        /// such a slot.
        ///
        /// This type exists because we want to expose many functions
        /// which *construct* a `base::Value` which needs to live somewhere.
        /// For that reason, we can't simply extract a reference
        /// to the underlying base::Value because it doesn't yet exist.
        /// Future interop tools should support constructing a base::Value
        /// in Rust and moving it (by value).
        /// TODO(crbug.com/1272780): Use bindgen-like tools to generate
        /// direct bindings to C++ constructors.
        type ValueSlot;

        // Construct a base::Value of the given type, with the given value.
        fn ConstructNoneValue(v: Pin<&mut ValueSlot>);
        fn ConstructBoolValue(v: Pin<&mut ValueSlot>, val: bool);
        fn ConstructIntegerValue(v: Pin<&mut ValueSlot>, val: i32);
        fn ConstructDoubleValue(v: Pin<&mut ValueSlot>, val: f64);
        fn ConstructStringValue(v: Pin<&mut ValueSlot>, value: &str);
        // Returns a reference to the base::Value within the
        // `absl::optional<base::Value>` (which is of type DICTIONARY)
        // so that it can be populated.
        fn ConstructDictValue(v: Pin<&mut ValueSlot>) -> Pin<&mut Value>;
        // Returns a reference to the base::Value within the
        // `absl::optional<base::Value>` (which is of type LIST)
        // so that it can be populated.
        fn ConstructListValue(v: Pin<&mut ValueSlot>) -> Pin<&mut Value>;

        fn DumpValueSlot(v: &ValueSlot) -> String;

        // Defined for Rust tests to crate a ValueSlot, because it requires C++ to do the creation
        // at this time.
        fn NewValueSlotForTesting() -> UniquePtr<ValueSlot>;
    }
}
