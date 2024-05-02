// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace switches {

// Delays execution of TaskPriority::BEST_EFFORT tasks until shutdown.
const char kDisableBestEffortTasks[] = "disable-best-effort-tasks";

// Disables the crash reporting.
const char kDisableBreakpad[]               = "disable-breakpad";

// Comma-separated list of feature names to disable. See also kEnableFeatures.
const char kDisableFeatures[] = "disable-features";

// Force disabling of low-end device mode when set.
const char kDisableLowEndDeviceMode[] = "disable-low-end-device-mode";

// Indicates that crash reporting should be enabled. On platforms where helper
// processes cannot access to files needed to make this decision, this flag is
// generated internally.
const char kEnableCrashReporter[]           = "enable-crash-reporter";

// Comma-separated list of feature names to enable. See also kDisableFeatures.
const char kEnableFeatures[] = "enable-features";

// Force low-end device mode when set.
const char kEnableLowEndDeviceMode[]        = "enable-low-end-device-mode";

// Enable the use of background thread priorities for background tasks in the
// ThreadPool even on systems where it is disabled by default, e.g. due to
// concerns about priority inversions.
const char kEnableBackgroundThreadPool[] = "enable-background-thread-pool";

// Handle to the shared memory segment containing field trial state that is to
// be shared between processes. The argument to this switch is made of segments
// separated by commas:
// - The platform-specific handle id for the shared memory as a string.
// - (Windows only) i=inherited by duplication or p=child must open parent.
// - The high 64 bits of the shared memory block GUID.
// - The low 64 bits of the shared memory block GUID.
// - The size of the shared memory segment as a string.
const char kFieldTrialHandle[] = "field-trial-handle";

// This option can be used to force field trials when testing changes locally.
// The argument is a list of name and value pairs, separated by slashes. If a
// trial name is prefixed with an asterisk, that trial will start activated.
// For example, the following argument defines two trials, with the second one
// activated: "GoogleNow/Enable/*MaterialDesignNTP/Default/" This option can
// also be used by the browser process to send the list of trials to a
// non-browser process, using the same format. See
// FieldTrialList::CreateTrialsFromString() in field_trial.h for details.
const char kForceFieldTrials[]              = "force-fieldtrials";

// Generates full memory crash dump.
const char kFullMemoryCrashReport[] = "full-memory-crash-report";

// Logs information about all tasks posted with TaskPriority::BEST_EFFORT. Use
// this to diagnose issues that are thought to be caused by
// TaskPriority::BEST_EFFORT execution fences. Note: Tasks posted to a
// non-BEST_EFFORT UpdateableSequencedTaskRunner whose priority is later lowered
// to BEST_EFFORT are not logged.
const char kLogBestEffortTasks[] = "log-best-effort-tasks";

// Handle to the shared memory segment a child process should use to transmit
// histograms back to the browser process.
const char kMetricsSharedMemoryHandle[] = "metrics-shmem-handle";

// Suppresses all error dialogs when present.
const char kNoErrorDialogs[]                = "noerrdialogs";

// Starts the sampling based profiler for the browser process at startup. This
// will only work if chrome has been built with the gn arg enable_profiling =
// true. The output will go to the value of kProfilingFile.
const char kProfilingAtStart[] = "profiling-at-start";

// Specifies a location for profiling output. This will only work if chrome has
// been built with the gyp variable profiling=1 or gn arg enable_profiling=true.
//
//   {pid} if present will be replaced by the pid of the process.
//   {count} if present will be incremented each time a profile is generated
//           for this process.
// The default is chrome-profile-{pid} for the browser and test-profile-{pid}
// for tests.
const char kProfilingFile[] = "profiling-file";

// Controls whether profile data is periodically flushed to a file. Normally
// the data gets written on exit but cases exist where chromium doesn't exit
// cleanly (especially when using single-process). A time in seconds can be
// specified.
const char kProfilingFlush[] = "profiling-flush";

// When running certain tests that spawn child processes, this switch indicates
// to the test framework that the current process is a child process.
const char kTestChildProcess[] = "test-child-process";

// Sends trace events from these categories to a file.
// --trace-to-file on its own sends to default categories.
const char kTraceToFile[] = "trace-to-file";

// Specifies the file name for --trace-to-file. If unspecified, it will
// go to a default file name.
const char kTraceToFileName[] = "trace-to-file-name";

// Gives the default maximal active V-logging level; 0 is the default.
// Normally positive values are used for V-logging levels.
const char kV[] = "v";

// Gives the per-module maximal V-logging levels to override the value
// given by --v.  E.g. "my_module=2,foo*=3" would change the logging
// level for all code in source files "my_module.*" and "foo*.*"
// ("-inl" suffixes are also disregarded for this matching).
//
// Any pattern containing a forward or backward slash will be tested
// against the whole pathname and not just the module.  E.g.,
// "*/foo/bar/*=2" would change the logging level for all code in
// source files under a "foo/bar" directory.
const char kVModule[] = "vmodule";

// Will wait for 60 seconds for a debugger to come to attach to the process.
const char kWaitForDebugger[] = "wait-for-debugger";

#if BUILDFLAG(IS_WIN)
// Disable high-resolution timer on Windows.
const char kDisableHighResTimer[] = "disable-highres-timer";

// Disables the USB keyboard detection for blocking the OSK on Windows.
const char kDisableUsbKeyboardDetect[]      = "disable-usb-keyboard-detect";
#endif

#if BUILDFLAG(IS_LINUX)
// The /dev/shm partition is too small in certain VM environments, causing
// Chrome to fail or crash (see http://crbug.com/715363). Use this flag to
// work-around this issue (a temporary directory will always be used to create
// anonymous shared memory files).
const char kDisableDevShmUsage[] = "disable-dev-shm-usage";
#endif

#if BUILDFLAG(IS_POSIX)
// Used for turning on Breakpad crash reporting in a debug environment where
// crash reporting is typically compiled but disabled.
const char kEnableCrashReporterForTesting[] =
    "enable-crash-reporter-for-testing";
#endif

#if BUILDFLAG(IS_ANDROID)
// Default country code to be used for search engine localization.
const char kDefaultCountryCodeAtInstall[] = "default-country-code";

// Adds additional thread idle time information into the trace event output.
const char kEnableIdleTracing[] = "enable-idle-tracing";

// The field trial parameters and their values when testing changes locally.
const char kForceFieldTrialParams[] = "force-fieldtrial-params";

// When we retrieve the package name within the SDK Runtime, we need to use
// a bit of a hack to do this by taking advantage of the fact that the pid
// is the same pid as the application's pid + 10000.
// see:
// https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/os/Process.java;l=292;drc=47fffdd53115a9af1820e3f89d8108745be4b55d
// When the render process is created however, it is just a regular isolated
// process with no particular association so we can't perform the same hack.
// When creating minidumps, the package name is retrieved from the process
// meaning the render process minidumps would end up reporting a generic
// process name not associated with the app.
// We work around this by feeding through the host package information to the
// render process when launching it.
const char kHostPackageName[] = "host-package-name";
const char kHostPackageLabel[] = "host-package-label";
const char kHostVersionCode[] = "host-version-code";
const char kPackageName[] = "package-name";
const char kPackageVersionName[] = "package-version-name";
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Override the default scheduling boosting value for urgent tasks.
// This can be adjusted if a specific chromeos device shows better perf/power
// ratio (e.g. by running video conference tests).
// Currently, this values directs to linux scheduler's utilization min clamp.
// Range is 0(no biased load) ~ 100(mamximum load value).
const char kSchedulerBoostUrgent[] = "scheduler-boost-urgent";
#endif

}  // namespace switches
