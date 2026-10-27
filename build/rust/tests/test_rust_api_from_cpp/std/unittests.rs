// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp/std:std_types_rs_api" as std_types;
}

use rust_gtest_interop::prelude::*;

// Crates that depend on `cpp_std` or on `rust_api_from_cpp` bindings should
// name C++ standard library types via `cpp_std` (and not via `cc_std`).  To
// verify that `cc_std` is not passed to such crates via `--extern`, the code
// below glob-imports an unrelated `cc_std` module.  If `cc_std` were also an
// `--extern` crate (e.g. because of `public_deps` instead of `deps`), then the
// `use cc_std::...` below would fail to compile with "error[E0659]: `cc_std`
// is ambiguous".
#[allow(dead_code)]
mod not_a_crate {
    pub mod cc_std {
        pub struct NotTheCcStdCrate;
    }
}
#[allow(unused_imports)]
use cc_std::NotTheCcStdCrate as _;
#[allow(unused_imports)]
use not_a_crate::*;

#[gtest(RustApiFromCppStd, OptionalParamsAndReturns)]
fn test_optional_params_and_returns() {
    // `std::optional<int>` parameters and return values use a layout-compatible
    // type from `cpp_std` (with explicit conversions from/to `Option<i32>`).
    let result: ::cpp_std::trivial_optional<i32> = ::std_types::AddOneIfPresent(Some(41).into());
    assert_eq!(Option::from(result), Some(42));
    let result = ::std_types::AddOneIfPresent(None.into());
    assert_eq!(Option::<i32>::from(result), None);
}

#[gtest(RustApiFromCppStd, OptionalField)]
fn test_optional_field() {
    // `std::optional<int>` fields use a layout-compatible type from `cpp_std`.
    let s = ::std_types::CreateStructWithOptionalField(123);
    let value: ::cpp_std::trivial_optional<i32> = s.value;
    assert_eq!(Option::from(value), Some(123));
}

#[gtest(RustApiFromCppStd, UniquePtr)]
fn test_unique_ptr() {
    use ::cpp_std::unique_ptr;
    use ::std_types::{CreateUniquePtr, GetValueFromUniquePtr};

    // `std::unique_ptr<int>` uses a layout-compatible type from `cpp_std`.
    let ptr: unique_ptr<i32> = CreateUniquePtr(123);
    assert_eq!(*ptr, 123);
    assert_eq!(GetValueFromUniquePtr(ptr), 123);

    // Ownership can move between C++ and Rust.  (Both sides allocate and free
    // through C++ `operator new` and `operator delete`, because Rust code uses
    // `cpp_std::Allocator`.)
    //
    // Memory allocated by C++ can be freed by Rust.
    let boxed = unique_ptr::into_box(CreateUniquePtr(456));
    assert_eq!(*boxed, 456);

    // Memory allocated by Rust can be freed by C++.
    assert_eq!(GetValueFromUniquePtr(unique_ptr::new(789)), 789);
}

#[gtest(RustApiFromCppStd, Vector)]
fn test_vector() {
    use ::cpp_std::vector;
    use ::std_types::{CreateVector, SumVector};

    // `std::vector<int>` uses a layout-compatible type from `cpp_std`.
    let mut v: vector<i32> = CreateVector(1, 2, 3);
    assert_eq!(v.as_slice(), &[1, 2, 3]);

    // Rust can grow a vector allocated by C++.  (Both sides allocate and free
    // through C++ `operator new` and `operator delete`, because Rust code uses
    // `cpp_std::Allocator`.)
    v.push(4);
    assert_eq!(v.len(), 4);
    assert_eq!(SumVector(v), 10);

    // Memory allocated by Rust can be freed by C++.
    assert_eq!(SumVector(vector::from(vec![5, 6, 7])), 18);
}
