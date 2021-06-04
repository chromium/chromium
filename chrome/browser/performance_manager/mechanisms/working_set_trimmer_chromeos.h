// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_CHROMEOS_H_

#include "base/callback_forward.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"

namespace performance_manager {

namespace policies {
class WorkingSetTrimmerPolicyChromeOS;
}  // namespace policies

namespace mechanism {

// WorkingSetTrimmerChromeOS is the platform specific implementation of a
// working set trimmer for ChromeOS. This class should not be used directly it
// should be used via the WorkingSetTrimmer::GetInstance() method.
class WorkingSetTrimmerChromeOS : public WorkingSetTrimmer {
 public:
  ~WorkingSetTrimmerChromeOS() override;

  // WorkingSetTrimmer implementation:
  bool PlatformSupportsWorkingSetTrim() override;
  bool TrimWorkingSet(const ProcessNode* process_node) override;

 private:
  friend class base::NoDestructor<WorkingSetTrimmerChromeOS>;
  friend class policies::WorkingSetTrimmerPolicyChromeOS;
  friend class TestWorkingSetTrimmerChromeOS;
  using TrimArcVmWorkingSetCallback =
      base::OnceCallback<void(bool result, const std::string& failure_reason)>;

  // TrimWorkingSet based on ProcessId |pid|.
  bool TrimWorkingSet(base::ProcessId pid);

  // Asks vm_concierge to trim ARCVM's memory in the same way as TrimWorkingSet.
  // The function must be called on the UI thread.
  void TrimArcVmWorkingSet(TrimArcVmWorkingSetCallback callback);

  // The constructor is made private to prevent instantiation of this class
  // directly, it should always be retrieved via
  // WorkingSetTrimmer::GetInstance().
  WorkingSetTrimmerChromeOS();

  DISALLOW_COPY_AND_ASSIGN(WorkingSetTrimmerChromeOS);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_CHROMEOS_H_
