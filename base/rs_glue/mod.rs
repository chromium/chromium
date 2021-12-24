// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// At present, none of this is used except in our Rust unit tests. Absolutely
// all of this is therefore #[cfg(test)] to avoid 'unused' warnings.

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
#[allow(unused)] // #[cfg(test) not supported here
pub(crate) mod ffi {
    unsafe extern "C++" {
        include!("base/rs_glue/values_glue.h");
        /// C++ `base::Value`
        #[namespace=base]
        type Value;

        // Free functions in C++ because none of the base::Value methods
        // precisely line up with what we need in Rust.

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

        // Set a given element of a base::Value of type LIST to the given
        // value.
        fn ValueSetNoneElement(v: Pin<&mut Value>, pos: usize);
        fn ValueSetBoolElement(v: Pin<&mut Value>, pos: usize, val: bool);
        fn ValueSetIntegerElement(v: Pin<&mut Value>, pos: usize, val: i32);
        fn ValueSetDoubleElement(v: Pin<&mut Value>, pos: usize, val: f64);
        fn ValueSetStringElement(v: Pin<&mut Value>, pos: usize, value: &str);
        // Returns a new child base::Value, of type DICTIONARY.
        fn ValueSetDictElement(v: Pin<&mut Value>, pos: usize) -> Pin<&mut Value>;
        // Returns a new child base::Value, of type LIST.
        fn ValueSetListElement(v: Pin<&mut Value>, pos: usize) -> Pin<&mut Value>;
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
        // The following is enabled in Rust unit tests only. cxx does
        // not allow us to use #[cfg(test)] attributes here. We could make a
        // separate ffi_test mod, but (unless we put it in a different file)
        // we would have to call the type something other than OptionalValue
        // to avoid conflicts, and that seems a worse solution.
        #[allow(unused)]
        fn NewValueSlot() -> UniquePtr<ValueSlot>;
    }
}
