// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:feature";
    "//base/test:scoped_feature_list";
}

use feature::{base_feature, base_feature_param, FeatureState, TimeDelta};
use rust_gtest_interop::prelude::*;
use scoped_feature_list::ScopedFeatureList;

base_feature!(FeatureOnByDefault, FeatureState::Enabled);
base_feature!(FeatureOffByDefault, FeatureState::Disabled);

base_feature!(FeatureWithParams, FeatureState::Disabled);
base_feature_param!(BoolParam, bool, &FeatureWithParams, "BoolParam", true);
base_feature_param!(IntParam, i32, &FeatureWithParams, "IntParam", 42);
base_feature_param!(DoubleParam, f64, &FeatureWithParams, "DoubleParam", 1.25);
base_feature_param!(StringParam, &'static str, &FeatureWithParams, "StringParam", "default_val");
base_feature_param!(
    TimeDeltaParam,
    TimeDelta,
    &FeatureWithParams,
    "TimeDeltaParam",
    TimeDelta::from_millis(500)
);

#[gtest(RustFeatureTest, DefaultStates)]
fn test_default_states() {
    expect_true!(FeatureOnByDefault.is_enabled());
    expect_false!(FeatureOffByDefault.is_enabled());
}

#[gtest(RustFeatureTest, FeatureParamDefaults)]
fn test_feature_param_defaults() {
    expect_true!(BoolParam.get());
    expect_eq!(IntParam.get(), 42);
    expect_eq!(DoubleParam.get(), 1.25);
    expect_eq!(StringParam.get(), "default_val");
    expect_eq!(TimeDeltaParam.get(), TimeDelta::from_millis(500));
}

#[gtest(RustFeatureTest, FeatureParamOverrides)]
fn test_feature_param_overrides() {
    let mut scoped_feature_list = ScopedFeatureList::new();
    scoped_feature_list.init_and_enable_feature_with_parameters(
        &FeatureWithParams,
        &[
            ("BoolParam", "false"),
            ("IntParam", "1234"),
            ("DoubleParam", "8.75"),
            ("StringParam", "custom_val"),
            ("TimeDeltaParam", "2s"),
        ],
    );

    expect_true!(FeatureWithParams.is_enabled());
    expect_false!(BoolParam.get());
    expect_eq!(IntParam.get(), 1234);
    expect_eq!(DoubleParam.get(), 8.75);
    expect_eq!(StringParam.get(), "custom_val");
    expect_eq!(TimeDeltaParam.get(), TimeDelta::from_secs(2));
}

#[gtest(RustFeatureTest, FeatureParamInvalidValuesFallback)]
fn test_feature_param_invalid_values_fallback() {
    let mut scoped_feature_list = ScopedFeatureList::new();
    scoped_feature_list.init_and_enable_feature_with_parameters(
        &FeatureWithParams,
        &[("DoubleParam", "not_a_double"), ("TimeDeltaParam", "not_a_duration")],
    );

    expect_eq!(DoubleParam.get(), 1.25);
    expect_eq!(TimeDeltaParam.get(), TimeDelta::from_millis(500));
}

#[gtest(RustFeatureTest, InitFromCommandLine)]
fn test_init_from_command_line() {
    struct TestCase {
        enable: &'static str,
        disable: &'static str,
        expected_on: bool,
        expected_off: bool,
    }

    let test_cases = [
        TestCase { enable: "", disable: "", expected_on: true, expected_off: false },
        TestCase {
            enable: "FeatureOffByDefault",
            disable: "",
            expected_on: true,
            expected_off: true,
        },
        TestCase {
            enable: "FeatureOffByDefault",
            disable: "FeatureOnByDefault",
            expected_on: false,
            expected_off: true,
        },
        TestCase {
            enable: "FeatureOnByDefault,FeatureOffByDefault",
            disable: "",
            expected_on: true,
            expected_off: true,
        },
        TestCase {
            enable: "",
            disable: "FeatureOnByDefault,FeatureOffByDefault",
            expected_on: false,
            expected_off: false,
        },
        // In the case an entry is both, disable takes precedence.
        TestCase {
            enable: "FeatureOnByDefault",
            disable: "FeatureOnByDefault,FeatureOffByDefault",
            expected_on: false,
            expected_off: false,
        },
    ];

    for test_case in test_cases {
        let mut scoped_feature_list = ScopedFeatureList::new();
        scoped_feature_list.init_from_command_line(test_case.enable, test_case.disable);

        expect_eq!(FeatureOnByDefault.is_enabled(), test_case.expected_on);
        expect_eq!(FeatureOffByDefault.is_enabled(), test_case.expected_off);
    }
}

#[gtest(RustFeatureTest, InitAndEnableFeature)]
fn test_init_and_enable_feature() {
    let mut scoped_feature_list = ScopedFeatureList::new();
    scoped_feature_list.init_and_enable_feature(&FeatureOffByDefault);
    expect_true!(FeatureOffByDefault.is_enabled());
}

#[gtest(RustFeatureTest, InitAndDisableFeature)]
fn test_init_and_disable_feature() {
    let mut scoped_feature_list = ScopedFeatureList::new();
    scoped_feature_list.init_and_disable_feature(&FeatureOnByDefault);
    expect_false!(FeatureOnByDefault.is_enabled());
}
