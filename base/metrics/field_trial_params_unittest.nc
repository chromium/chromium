// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

constexpr base::Feature kFeature{"NoCompileFeature"};

enum Param { FOO, BAR };

#if defined(NCTEST_NO_PARAM_TYPE)  // [r"too few template arguments"]

constexpr base::FeatureParam<> kParam{
  &kFeature, "Param"};

#elif defined(NCTEST_VOID_PARAM_TYPE)  // [r"unsupported FeatureParam<> type"]

constexpr base::FeatureParam<void> kParam{
  &kFeature, "Param"};

#elif defined(NCTEST_INVALID_PARAM_TYPE)  // [r"unsupported FeatureParam<> type"]

constexpr base::FeatureParam<size_t> kParam{
  &kFeature, "Param", 1u};

#elif defined(NCTEST_ENUM_NULL_OPTIONS)  // [r"candidate template ignored: could not match"]

constexpr base::FeatureParam<Param> kParam{
  &kFeature, "Param", FOO, nullptr};

#elif defined(NCTEST_ENUM_EMPTY_OPTIONS)  // [r"zero-length arrays are not permitted"]

constexpr base::FeatureParam<Param>::Option kParamOptions[] = {};
constexpr base::FeatureParam<Param> kParam{
  &kFeature, "Param", FOO, &kParamOptions};

#else

void suppress_unused_variable_warning() {
    (void)kFeature;
}

#endif
