// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace base {

constexpr Feature kFeature{"NoCompileFeature", FEATURE_DISABLED_BY_DEFAULT};

// Must supply an enum template argument.
constexpr FeatureParam<> kParam1{&kFeature, "Param"};            // expected-error {{too few template arguments}}
constexpr FeatureParam<void> kParam2{&kFeature, "Param"};        // expected-error@*:* {{unsupported FeatureParam<> type}}
constexpr FeatureParam<size_t> kParam3{&kFeature, "Param", 1u};  // expected-error@*:* {{unsupported FeatureParam<> type}}

enum Param { kFoo, kBar };

// Options pointer must be non-null.
constexpr FeatureParam<Param> kParam4{&kFeature, "Param", kFoo, nullptr};  // expected-error {{no matching constructor}}

constexpr FeatureParam<Param>::Option kParamOptions[] = {};
constexpr FeatureParam<Param> kParam5{&kFeature, "Param", kFoo, &kParamOptions};  // expected-error {{no matching constructor}}

}  // namespace base
