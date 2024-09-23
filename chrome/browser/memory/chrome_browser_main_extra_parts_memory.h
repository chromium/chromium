// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_CHROME_BROWSER_MAIN_EXTRA_PARTS_MEMORY_H_
#define CHROME_BROWSER_MEMORY_CHROME_BROWSER_MAIN_EXTRA_PARTS_MEMORY_H_

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
namespace memory {
class SystemMemoryPressureEvaluator;
}
}  // namespace ash
#endif

namespace memory {
class EnterpriseMemoryLimitPrefObserver;
}  // namespace memory

// Wrapper that owns and initialize the browser memory-related extra parts.
class ChromeBrowserMainExtraPartsMemory : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsMemory();

  ChromeBrowserMainExtraPartsMemory(const ChromeBrowserMainExtraPartsMemory&) =
      delete;
  ChromeBrowserMainExtraPartsMemory& operator=(
      const ChromeBrowserMainExtraPartsMemory&) = delete;

  ~ChromeBrowserMainExtraPartsMemory() override;

 private:
  // ChromeBrowserMainExtraParts overrides.
  void PostCreateThreads() override;
  void PostBrowserStart() override;
  void PostMainMessageLoopRun() override;

  // Tracks changes to the MemoryLimitMbEnabled enterprise policy, and
  // starts/stops the EnterpriseMemoryLimitEvaluator accordingly.
  //
  // Only supported on some platforms, see
  // EnterpriseMemoryLimitPrefObserver::PlatformIsSupported for the list of
  // supported platforms.
  std::unique_ptr<memory::EnterpriseMemoryLimitPrefObserver>
      memory_limit_pref_observer_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::memory::SystemMemoryPressureEvaluator> cros_evaluator_;
#endif
};

#endif  // CHROME_BROWSER_MEMORY_CHROME_BROWSER_MAIN_EXTRA_PARTS_MEMORY_H_
