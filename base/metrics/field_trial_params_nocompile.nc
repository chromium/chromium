// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace base {

BASE_FEATURE(kNoCompileFeature, "NoCompileFeature", FEATURE_DISABLED_BY_DEFAULT);

// Must supply an enum template argument.
constexpr FeatureParam<> kParam1{&kNoCompileFeature, "Param"};            // expected-error {{too few template arguments}}
constexpr FeatureParam<void> kParam2{&kNoCompileFeature, "Param"};        // expected-error@*:* {{Unsupported FeatureParam<> type}}

enum Param { kFoo, kBar };

// Options pointer must be non-null.
constexpr FeatureParam<Param> kParam4{&kNoCompileFeature, "Param", kFoo, nullptr};  // expected-error {{no matching constructor}}

constexpr FeatureParam<Param>::Option kParamOptions[] = {};
constexpr FeatureParam<Param> kParam5{&kNoCompileFeature, "Param", kFoo, &kParamOptions};  // expected-error {{no matching constructor}}

}  // namespace base
