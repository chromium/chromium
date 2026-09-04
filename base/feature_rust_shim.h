// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FEATURE_RUST_SHIM_H_
#define BASE_FEATURE_RUST_SHIM_H_

#include <stdint.h>

#include "base/feature.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace base {

bool GetFieldTrialParamByFeatureAsBoolShim(const Feature& feature,
                                           ::rust::Str param_name,
                                           bool default_value);

int32_t GetFieldTrialParamByFeatureAsIntShim(const Feature& feature,
                                             ::rust::Str param_name,
                                             int32_t default_value);

}  // namespace base

#endif  // BASE_FEATURE_RUST_SHIM_H_
