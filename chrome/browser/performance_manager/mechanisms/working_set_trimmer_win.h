// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_WIN_H_

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"

namespace performance_manager {
namespace mechanism {

// WorkingSetTrimmerWin is the platform specific implementation of a
// working set trimmer for Windows. This class should not be used directly it
// should be used via the WorkingSetTrimmer::GetIntsance() method.
class WorkingSetTrimmerWin : public WorkingSetTrimmer {
 public:
  ~WorkingSetTrimmerWin() override;

  bool PlatformSupportsWorkingSetTrim() override;
  bool TrimWorkingSet(const ProcessNode* process_node) override;

 private:
  friend class base::NoDestructor<WorkingSetTrimmerWin>;

  // The constructor is made private to prevent instantiation of this class
  // directly, it should always be retrieved via
  // WorkingSetTrimmer::GetIntsance().
  WorkingSetTrimmerWin();

  DISALLOW_COPY_AND_ASSIGN(WorkingSetTrimmerWin);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_WIN_H_
