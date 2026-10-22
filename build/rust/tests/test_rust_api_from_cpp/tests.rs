// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp:assume_lifetimes_rs_api"
        as assume_lifetimes;
    "//build/rust/tests/test_rust_api_from_cpp:explicit_lifetimes_rs_api"
        as explicit_lifetimes;
    "//build/rust/tests/test_rust_api_from_cpp:lifetime_bound_rs_api"
        as lifetime_bound;
    "//build/rust/tests/test_rust_api_from_cpp:self_contained_target_rs_api"
        as self_contained_target;
    "//build/rust/tests/test_rust_api_from_cpp:target_depending_on_another_rs_api"
        as target_depending_on_another;
    "//build/rust/tests/test_rust_api_from_cpp:test_browsing_instance_id_rs_api"
        as test_browsing_instance_id;
    "//build/rust/tests/test_rust_api_from_cpp:test_extra_rust_source_groups_rs_api"
        as test_extra_rust_source_groups;
    "//build/rust/tests/test_rust_api_from_cpp:test_std_types_rs_api"
        as test_std_types;
    "//build/rust/tests/test_rust_api_from_cpp:test_time_delta_rs_api"
        as test_time_delta;
}

use rust_gtest_interop::prelude::*;

#[gtest(TestRustApiFromCpp, FunctionCalls)]
fn test_self_contained_target_function_call_basics() {
    assert_eq!(100 + 42, ::self_contained_target::AddViaCc(100, 42));
    assert_eq!(100 * 42, ::self_contained_target::MultiplyViaCc(100, 42));
}

#[gtest(TestRustApiFromCpp, PodStruct)]
fn test_self_contained_target_pod_struct_basics() {
    let x = ::self_contained_target::CcPodStruct { value: 123 };
    assert_eq!(x.value, 123);
}

#[gtest(TestRustApiFromCpp, TargetDependingOnAnother)]
fn test_target_depending_on_another() {
    let x = ::target_depending_on_another::CreateCcPodStructFromValue(456);
    assert_eq!(x.value, 456);
}

#[gtest(TestRustApiFromCpp, AssumeLifetimes)]
fn test_assume_lifetimes() {
    // By default, C++ methods take `&self` or `&mut self` in Rust (and
    // therefore can be called with the method-call syntax).
    let mut x = ::assume_lifetimes::AssumeLifetimesStruct { value: 123 };
    assert_eq!(x.GetValue(), 123);
    x.SetValue(456);
    assert_eq!(x.GetValue(), 456);
}

#[gtest(TestRustApiFromCpp, LifetimeBound)]
fn test_lifetime_bound() {
    // `LIFETIME_BOUND` annotations tie the result of `GetLargerValue` to both
    // `self` and `other`.  (Lifetime elision would only tie the result to
    // `self`, which would let the result outlive `other`.)
    let x = ::lifetime_bound::LifetimeBoundStruct { value: 123 };
    let other = 456;
    let larger = x.GetLargerValue(&other);
    assert!(std::ptr::eq(::cref::CRef::as_ptr(larger), &other));
}

#[gtest(TestRustApiFromCpp, ExplicitLifetimes)]
fn test_explicit_lifetimes() {
    // Explicit lifetime annotations tie the result of `GetLargerValue` to
    // both `self` and `other`.  (Lifetime elision would only tie the result to
    // `self`, which would let the result outlive `other`.)
    let x = ::explicit_lifetimes::ExplicitLifetimesStruct { value: 123 };
    let other = 456;
    let larger = x.GetLargerValue(&other);
    assert!(std::ptr::eq(::cref::CRef::as_ptr(larger), &other));
}

#[gtest(TestRustApiFromCpp, ExtraRustSources)]
fn test_extra_rust_sources() {
    use ::test_time_delta::test_base::TestTimeDelta;
    use std::time::Duration;

    // `TryFrom` impl comes from `extra_rust_sources`.
    let delta = TestTimeDelta::FromMicroseconds(1_234_567);
    assert_eq!(Duration::try_from(delta), Ok(Duration::from_micros(1_234_567)));

    let negative_delta = TestTimeDelta::FromMicroseconds(-1);
    assert!(Duration::try_from(negative_delta).is_err());
}

#[gtest(TestRustApiFromCpp, ExtraRustSourceGroups)]
fn test_extra_rust_source_groups() {
    use ::test_extra_rust_source_groups::test_outer;
    use ::test_extra_rust_source_groups::test_outer::test_inner;

    // `double_meters` comes from an `extra_rust_source_groups` entry placed
    // into the `test_outer::test_inner` module.  It calls `scale_factor`,
    // which comes from `extra_rust_sources` (placed at the root of the crate).
    let m = test_inner::double_meters(test_inner::TestMeters { value: 21 });
    assert_eq!(m.value, 42);
    assert_eq!(::test_extra_rust_source_groups::scale_factor(), 2);

    // `triple_meters` comes from another `extra_rust_source_groups` entry
    // (placed into the `test_outer` module).
    let m = test_outer::triple_meters(test_inner::TestMeters { value: 14 });
    assert_eq!(m.value, 42);
}

#[gtest(TestRustApiFromCpp, StdOptionalParamsAndReturns)]
fn test_std_optional_params_and_returns() {
    // `std::optional<int>` parameters and return values use a layout-compatible
    // type from `cc_std` (with explicit conversions from/to `Option<i32>`).
    let result = ::test_std_types::AddOneIfPresent(Some(41).into());
    assert_eq!(Option::from(result), Some(42));
    let result = ::test_std_types::AddOneIfPresent(None.into());
    assert_eq!(Option::<i32>::from(result), None);
}

#[gtest(TestRustApiFromCpp, StdOptionalField)]
fn test_std_optional_field() {
    // `std::optional<int>` fields use a layout-compatible type from `cc_std`.
    let s = ::test_std_types::CreateStructWithOptionalField(123);
    let value: ::cc_std::std::trivial_optional<i32> = s.value;
    assert_eq!(Option::from(value), Some(123));
}

#[gtest(TestRustApiFromCpp, StdUniquePtr)]
fn test_std_unique_ptr() {
    use ::cc_std::std::unique_ptr;
    use ::test_std_types::{CreateUniquePtr, GetValueFromUniquePtr};

    // `std::unique_ptr<int>` uses a layout-compatible type from `cc_std`.
    let ptr: unique_ptr<i32> = CreateUniquePtr(123);
    assert_eq!(*ptr, 123);
    assert_eq!(GetValueFromUniquePtr(ptr), 123);

    // Ownership can move between C++ and Rust.  (Both sides allocate and free
    // through C++ `operator new` and `operator delete`, because Rust code uses
    // `cc_std::std::Allocator`.)
    //
    // Memory allocated by C++ can be freed by Rust.
    let boxed = unique_ptr::into_box(CreateUniquePtr(456));
    assert_eq!(*boxed, 456);

    // Memory allocated by Rust can be freed by C++.
    assert_eq!(GetValueFromUniquePtr(unique_ptr::new(789)), 789);
}

#[gtest(TestRustApiFromCpp, StdVector)]
fn test_std_vector() {
    use ::cc_std::std::vector;
    use ::test_std_types::{CreateVector, SumVector};

    // `std::vector<int>` uses a layout-compatible type from `cc_std`.
    let mut v: vector<i32> = CreateVector(1, 2, 3);
    assert_eq!(v.as_slice(), &[1, 2, 3]);

    // Rust can grow a vector allocated by C++.  (Both sides allocate and free
    // through C++ `operator new` and `operator delete`, because Rust code uses
    // `cc_std::std::Allocator`.)
    v.push(4);
    assert_eq!(v.len(), 4);
    assert_eq!(SumVector(v), 10);

    // Memory allocated by Rust can be freed by C++.
    assert_eq!(SumVector(vector::from(vec![5, 6, 7])), 18);
}

#[gtest(TestRustApiFromCpp, TemplateInstantiation)]
fn test_template_instantiation() {
    use ::test_browsing_instance_id::{
        CreateTestBrowsingInstanceId, GetTestBrowsingInstanceIdValue, TestBrowsingInstanceId,
    };

    // `TestBrowsingInstanceId` is a type alias for a class template
    // specialization: `base::IdType32<TestBrowsingInstanceIdTag>`.
    let id: TestBrowsingInstanceId = CreateTestBrowsingInstanceId(123);
    assert_eq!(GetTestBrowsingInstanceIdValue(id), 123);

    // `Default` comes from the default constructor of the class template.
    let null_id = TestBrowsingInstanceId::default();
    assert_eq!(GetTestBrowsingInstanceIdValue(null_id), 0);
}
