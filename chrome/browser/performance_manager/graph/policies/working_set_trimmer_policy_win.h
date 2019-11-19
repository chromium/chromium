// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_WORKING_SET_TRIMMER_POLICY_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_WORKING_SET_TRIMMER_POLICY_WIN_H_

#include "chrome/browser/performance_manager/graph/policies/working_set_trimmer_policy.h"

namespace performance_manager {
namespace policies {

// The Windows WorkingSetTrimmerPolicy uses the defaults except it defines it's
// own PlatformSupportsWorkingSetTrim() because we want to flag guard this
// feature.
class WorkingSetTrimmerPolicyWin : public WorkingSetTrimmerPolicy {
 public:
  ~WorkingSetTrimmerPolicyWin() override;
  WorkingSetTrimmerPolicyWin();

  // Returns true if this platform supports working set trim, in the case of
  // Windows this will check that the appropriate flags are set for working set
  // trim.
  static bool PlatformSupportsWorkingSetTrim();

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkingSetTrimmerPolicyWin);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_WORKING_SET_TRIMMER_POLICY_WIN_H_
