// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_H_

#include "base/no_destructor.h"

namespace performance_manager {

class ProcessNode;

namespace mechanism {

// A WorkingSetTrimmer will reduce a ProcessNode's memory footprint by giving a
// hint to the operating system that this processes memory may be reclaimed or
// trimmed.
class WorkingSetTrimmer {
 public:
  WorkingSetTrimmer(const WorkingSetTrimmer&) = delete;
  WorkingSetTrimmer& operator=(const WorkingSetTrimmer&) = delete;

  virtual ~WorkingSetTrimmer() = default;

  // GetInstance will return the singleton instance of a working set trimmer for
  // this platform.
  static WorkingSetTrimmer* GetInstance();

  // Returns true if the WorkingSetTrimmer is supported on the current platform.
  virtual bool PlatformSupportsWorkingSetTrim() = 0;

  virtual void TrimWorkingSet(const ProcessNode* process_node) = 0;

 protected:
  // A WorkingSetTrimmer should never be created directly it should only be
  // retrieved via WorkingSetTrimmer::GetInstance().
  WorkingSetTrimmer() = default;

 private:
  friend class base::NoDestructor<WorkingSetTrimmer>;
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_WORKING_SET_TRIMMER_H_
