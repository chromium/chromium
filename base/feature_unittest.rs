// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:feature";
    "//base/test:scoped_feature_list";
}

use feature::{base_feature, FeatureState};
use rust_gtest_interop::prelude::*;
use scoped_feature_list::ScopedFeatureList;

base_feature!(FeatureOnByDefault, FeatureState::Enabled);
base_feature!(FeatureOffByDefault, FeatureState::Disabled);

#[gtest(RustFeatureTest, DefaultStates)]
fn test_default_states() {
    expect_true!(FeatureOnByDefault.is_enabled());
    expect_false!(FeatureOffByDefault.is_enabled());
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
