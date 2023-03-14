// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"
#endif

namespace performance_manager {
namespace mechanism {
namespace {

// The NoOpWorkingSetTrimmer provides an implementation of a working set trimmer
// that does nothing on unsupported platforms.
class NoOpWorkingSetTrimmer : public WorkingSetTrimmer {
 public:
  ~NoOpWorkingSetTrimmer() override = default;
  NoOpWorkingSetTrimmer() = default;

  // WorkingSetTrimmer implementation:
  bool PlatformSupportsWorkingSetTrim() override { return false; }
  void TrimWorkingSet(const ProcessNode* node) override {}
};

}  // namespace

WorkingSetTrimmer* WorkingSetTrimmer::GetInstance() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  static base::NoDestructor<WorkingSetTrimmerChromeOS> trimmer;
#else
  static base::NoDestructor<NoOpWorkingSetTrimmer> trimmer;
#endif
  return trimmer.get();
}

}  // namespace mechanism
}  // namespace performance_manager
