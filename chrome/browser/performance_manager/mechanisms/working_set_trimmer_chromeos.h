// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_CHROMEOS_H_

#include "ash/components/arc/mojom/memory.mojom-forward.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {
enum class ArcVmReclaimType;
}
namespace performance_manager {

namespace policies {
class WorkingSetTrimmerPolicyChromeOS;
}  // namespace policies

namespace mechanism {

// For name compatibility of mechanism::ArcVmReclaimType.
using arc::ArcVmReclaimType;

// WorkingSetTrimmerChromeOS is the platform specific implementation of a
// working set trimmer for ChromeOS. This class should not be used directly it
// should be used via the WorkingSetTrimmer::GetInstance() method.
class WorkingSetTrimmerChromeOS : public WorkingSetTrimmer {
 public:
  WorkingSetTrimmerChromeOS(const WorkingSetTrimmerChromeOS&) = delete;
  WorkingSetTrimmerChromeOS& operator=(const WorkingSetTrimmerChromeOS&) =
      delete;

  ~WorkingSetTrimmerChromeOS() override;

  // WorkingSetTrimmer implementation:
  bool PlatformSupportsWorkingSetTrim() override;
  void TrimWorkingSet(const ProcessNode* process_node) override;

 private:
  friend class base::NoDestructor<WorkingSetTrimmerChromeOS>;
  friend class policies::WorkingSetTrimmerPolicyChromeOS;
  friend class TestWorkingSetTrimmerChromeOS;
  friend class MockWorkingSetTrimmerChromeOS;
  using TrimArcVmWorkingSetCallback =
      base::OnceCallback<void(bool result, const std::string& failure_reason)>;

  // Creates the trimmer with the |context| for testing.
  static std::unique_ptr<WorkingSetTrimmerChromeOS> CreateForTesting(
      content::BrowserContext* context);

  // TrimWorkingSet based on ProcessId |pid|.
  void TrimWorkingSet(base::ProcessId pid);

  // Note: made virtual to ease unit testing (redefine in derived mock).
  virtual void TrimArcVmWorkingSet(TrimArcVmWorkingSetCallback callback,
                                   ArcVmReclaimType reclaim_type,
                                   int page_limit);

  // The constructor is made private to prevent instantiation of this class
  // directly, it should always be retrieved via
  // WorkingSetTrimmer::GetInstance().
  WorkingSetTrimmerChromeOS();

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<content::BrowserContext> context_for_testing_ = nullptr;

  base::WeakPtrFactory<WorkingSetTrimmerChromeOS> weak_factory_{this};
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_CHROMEOS_H_
