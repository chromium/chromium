// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_win.h"

#include "base/feature_list.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"

namespace performance_manager {
namespace policies {

WorkingSetTrimmerPolicyWin::WorkingSetTrimmerPolicyWin() = default;
WorkingSetTrimmerPolicyWin::~WorkingSetTrimmerPolicyWin() = default;

// static
bool WorkingSetTrimmerPolicyWin::PlatformSupportsWorkingSetTrim() {
  bool enabled = base::FeatureList::IsEnabled(features::kEmptyWorkingSet);
  bool supported = mechanism::WorkingSetTrimmer::GetInstance()
                       ->PlatformSupportsWorkingSetTrim();

  return enabled && supported;
}

}  // namespace policies
}  // namespace performance_manager
