// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp/forward_declare:definition_rs_api"
        as definition;
    "//build/rust/tests/test_rust_api_from_cpp/forward_declare:usage_rs_api"
        as usage;
}

use ::definition::TestForwardDeclaredStruct;
use ::forward_declare::CppCoerce;
use ::usage::{
    GetValueViaForwardDeclaration, PassThroughForwardDeclaration, SetValueViaForwardDeclaration,
};
use rust_gtest_interop::prelude::*;

// `GetValueViaForwardDeclaration` takes a `const&` to a forward-declared
// (incomplete) C++ type.  `cpp_coerce` converts a reference to the full
// definition into a reference to the incomplete type.
#[gtest(RustApiFromCppForwardDeclare, ConstRef)]
fn test_const_ref() {
    let s = TestForwardDeclaredStruct { value: 123 };
    expect_eq!(GetValueViaForwardDeclaration((&s).cpp_coerce()), 123);
}

// `SetValueViaForwardDeclaration` takes a mutable reference to
// a forward-declared (incomplete) C++ type.
#[gtest(RustApiFromCppForwardDeclare, MutRef)]
fn test_mut_ref() {
    let mut s = TestForwardDeclaredStruct { value: 123 };
    SetValueViaForwardDeclaration((&mut s).cpp_coerce(), 456);
    expect_eq!(s.value, 456);
}

// `PassThroughForwardDeclaration` returns a `const&` to a forward-declared
// (incomplete) C++ type.  `cpp_coerce` can also convert such a reference back
// into a reference to the full definition.
#[gtest(RustApiFromCppForwardDeclare, ReturnedConstRef)]
fn test_returned_const_ref() {
    let s = TestForwardDeclaredStruct { value: 123 };
    let r: &TestForwardDeclaredStruct =
        PassThroughForwardDeclaration((&s).cpp_coerce()).cpp_coerce();
    expect_eq!(r.value, 123);
    expect_true!(std::ptr::eq(r, &s));
}
