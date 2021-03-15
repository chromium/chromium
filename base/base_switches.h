// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "base" command-line switches.

#ifndef BASE_BASE_SWITCHES_H_
#define BASE_BASE_SWITCHES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace switches {

extern const char kDisableBestEffortTasks[];
extern const char kDisableBreakpad[];
extern const char kDisableFeatures[];
extern const char kDisableLowEndDeviceMode[];
extern const char kEnableCrashReporter[];
extern const char kEnableFeatures[];
extern const char kEnableLowEndDeviceMode[];
extern const char kEnableBackgroundThreadPool[];
extern const char kForceFieldTrials[];
extern const char kFullMemoryCrashReport[];
extern const char kLogBestEffortTasks[];
extern const char kNoErrorDialogs[];
extern const char kProfilingAtStart[];
extern const char kProfilingFile[];
extern const char kProfilingFlush[];
extern const char kTestChildProcess[];
extern const char kTestDoNotInitializeIcu[];
extern const char kTraceToFile[];
extern const char kTraceToFileName[];
extern const char kV[];
extern const char kVModule[];
extern const char kWaitForDebugger[];

#if defined(OS_WIN)
extern const char kDisableHighResTimer[];
extern const char kDisableUsbKeyboardDetect[];
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    !BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kDisableDevShmUsage[];
#endif

#if defined(OS_POSIX)
extern const char kEnableCrashReporterForTesting[];
#endif

#if defined(OS_ANDROID)
extern const char kEnableReachedCodeProfiler[];
extern const char kReachedCodeSamplingIntervalUs[];
extern const char kDefaultCountryCodeAtInstall[];
extern const char kEnableIdleTracing[];
extern const char kForceFieldTrialParams[];
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
extern const char kEnableThreadInstructionCount[];

// TODO(crbug.com/1176772): Remove kEnableCrashpad and IsCrashpadEnabled() when
// Crashpad is fully enabled on Linux.
extern const char kEnableCrashpad[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kSchedulerBoostUrgent[];
#endif

}  // namespace switches

#endif  // BASE_BASE_SWITCHES_H_
