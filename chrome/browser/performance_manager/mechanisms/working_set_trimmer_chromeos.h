// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_CHROMEOS_H_

#include "base/no_destructor.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"

namespace performance_manager {
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

  // The constructor is made private to prevent instantiation of this class
  // directly, it should always be retrieved via
  // WorkingSetTrimmer::GetInstance().
  WorkingSetTrimmerChromeOS();

  DISALLOW_COPY_AND_ASSIGN(WorkingSetTrimmerChromeOS);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_CHROMEOS_H_
