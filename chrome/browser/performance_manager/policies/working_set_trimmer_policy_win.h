// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_WIN_H_

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy.h"

namespace performance_manager {
namespace policies {

// The Windows WorkingSetTrimmerPolicy uses the defaults except it defines it's
// own PlatformSupportsWorkingSetTrim() because we want to flag guard this
// feature.
class WorkingSetTrimmerPolicyWin : public WorkingSetTrimmerPolicy {
 public:
  WorkingSetTrimmerPolicyWin(const WorkingSetTrimmerPolicyWin&) = delete;
  WorkingSetTrimmerPolicyWin& operator=(const WorkingSetTrimmerPolicyWin&) =
      delete;

  ~WorkingSetTrimmerPolicyWin() override;
  WorkingSetTrimmerPolicyWin();

  // Returns true if this platform supports working set trim, in the case of
  // Windows this will check that the appropriate flags are set for working set
  // trim.
  static bool PlatformSupportsWorkingSetTrim();
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_WIN_H_
