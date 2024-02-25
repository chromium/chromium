// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/chrome_browser_main_extra_parts_memory.h"

#include "base/memory/memory_pressure_monitor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/memory/enterprise_memory_limit_pref_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/memory/pressure/system_memory_pressure_evaluator.h"
#endif

ChromeBrowserMainExtraPartsMemory::ChromeBrowserMainExtraPartsMemory() = default;

ChromeBrowserMainExtraPartsMemory::~ChromeBrowserMainExtraPartsMemory() =
    default;

void ChromeBrowserMainExtraPartsMemory::PostBrowserStart() {
  // The MemoryPressureMonitor might not be available in some tests.
  if (base::MemoryPressureMonitor::Get()) {
    if (memory::EnterpriseMemoryLimitPrefObserver::PlatformIsSupported()) {
      memory_limit_pref_observer_ =
          std::make_unique<memory::EnterpriseMemoryLimitPrefObserver>(
              g_browser_process->local_state());
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (base::SysInfo::IsRunningOnChromeOS()) {
      cros_evaluator_ =
          std::make_unique<ash::memory::SystemMemoryPressureEvaluator>(
              static_cast<memory_pressure::MultiSourceMemoryPressureMonitor*>(
                  base::MemoryPressureMonitor::Get())
                  ->CreateVoter());
    }
#endif
  }
}

void ChromeBrowserMainExtraPartsMemory::PostMainMessageLoopRun() {
  // |memory_limit_pref_observer_| must be destroyed before its |pref_service_|
  // is destroyed, as the observer's PrefChangeRegistrar's destructor uses the
  // pref_service.
  memory_limit_pref_observer_.reset();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  cros_evaluator_.reset();
#endif
}
