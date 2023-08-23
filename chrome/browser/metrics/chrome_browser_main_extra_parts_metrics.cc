// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/allocator/partition_alloc_support.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/sparse_histogram.h"
#include "base/power_monitor/power_monitor_buildflags.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/compiler/compiler_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/process_memory_metrics_emitter.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_switches.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/metrics/android_metrics_helper.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info_values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "crypto/unexportable_key_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/power_monitor/battery_state_sampler.h"
#include "chrome/browser/metrics/first_web_contents_profiler.h"
#include "chrome/browser/metrics/power/battery_discharge_reporter.h"
#include "chrome/browser/metrics/power/power_metrics_reporter.h"
#include "chrome/browser/metrics/power/process_monitor.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#if defined(__arm__)
#include <cpu-features.h>
#endif
#include "base/android/build_info.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#endif  // BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(__GLIBC__) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include <gnu/libc-version.h>

#include "base/linux_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_OZONE)
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"
#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/win/base_win_buildflags.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/installer/util/taskbar_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/metrics/pressure/pressure_metrics_reporter.h"
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/power_metrics/system_power_monitor.h"
#endif

namespace {

// The number of restarts to wait until removing the enable-benchmarking flag.
constexpr int kEnableBenchmarkingCountdownDefault = 3;
constexpr char kEnableBenchmarkingPrefId[] = "enable_benchmarking_countdown";

void RecordMemoryMetrics();

// Gets the delay for logging memory related metrics for testing.
absl::optional<base::TimeDelta> GetDelayForNextMemoryLogTest() {
  int test_delay_in_minutes;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestMemoryLogDelayInMinutes) &&
      base::StringToInt(command_line->GetSwitchValueASCII(
                            switches::kTestMemoryLogDelayInMinutes),
                        &test_delay_in_minutes)) {
    return base::Minutes(test_delay_in_minutes);
  }
  return absl::nullopt;
}

// Records memory metrics after a delay.
void RecordMemoryMetricsAfterDelay() {
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RecordMemoryMetrics),
      GetDelayForNextMemoryLogTest().value_or(
          memory_instrumentation::GetDelayForNextMemoryLog()));
}

// Records memory metrics, and then triggers memory colleciton after a delay.
void RecordMemoryMetrics() {
  scoped_refptr<ProcessMemoryMetricsEmitter> emitter(
      new ProcessMemoryMetricsEmitter);
  emitter->FetchAndEmitProcessMemoryMetrics();

  RecordMemoryMetricsAfterDelay();
}

// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum UMALinuxDistro {
  UMA_LINUX_DISTRO_UNKNOWN = 0,
  UMA_LINUX_DISTRO_ARCH = 1,
  UMA_LINUX_DISTRO_CENTOS = 2,
  UMA_LINUX_DISTRO_DEBIAN = 3,
  UMA_LINUX_DISTRO_ELEMENTARY = 4,
  UMA_LINUX_DISTRO_FEDORA = 5,
  UMA_LINUX_DISTRO_MINT = 6,
  UMA_LINUX_DISTRO_OPENSUSE_LEAP = 7,
  UMA_LINUX_DISTRO_RHEL = 8,
  UMA_LINUX_DISTRO_SUSE_ENTERPRISE = 9,
  UMA_LINUX_DISTRO_UBUNTU = 10,

  // Note: Add new distros to the list above this line, and update Linux.Distro2
  // in tools/metrics/histograms/enums.xml accordingly.
  UMA_LINUX_DISTRO_MAX
};

enum UMALinuxGlibcVersion {
  UMA_LINUX_GLIBC_NOT_PARSEABLE,
  UMA_LINUX_GLIBC_UNKNOWN,
  UMA_LINUX_GLIBC_2_11,
  // To log newer versions, just update tools/metrics/histograms/histograms.xml.
};

enum UMATouchEventFeatureDetectionState {
  UMA_TOUCH_EVENT_FEATURE_DETECTION_ENABLED,
  UMA_TOUCH_EVENT_FEATURE_DETECTION_AUTO_ENABLED,
  UMA_TOUCH_EVENT_FEATURE_DETECTION_AUTO_DISABLED,
  UMA_TOUCH_EVENT_FEATURE_DETECTION_DISABLED,
  // NOTE: Add states only immediately above this line. Make sure to
  // update the enum list in tools/metrics/histograms/histograms.xml
  // accordingly.
  UMA_TOUCH_EVENT_FEATURE_DETECTION_STATE_COUNT
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class ChromeOSChannel {
  kUnknown = 0,
  kCanary = 1,
  kDev = 2,
  kBeta = 3,
  kStable = 4,
  kMaxValue = kStable,
};

// Records the underlying Chrome OS release channel, which may be different than
// the Lacros browser's release channel.
void RecordChromeOSChannel() {
  ChromeOSChannel os_channel = ChromeOSChannel::kUnknown;
  std::string release_track;
  if (base::SysInfo::GetLsbReleaseValue(crosapi::kChromeOSReleaseTrack,
                                        &release_track)) {
    if (release_track == crosapi::kReleaseChannelStable)
      os_channel = ChromeOSChannel::kStable;
    else if (release_track == crosapi::kReleaseChannelBeta)
      os_channel = ChromeOSChannel::kBeta;
    else if (release_track == crosapi::kReleaseChannelDev)
      os_channel = ChromeOSChannel::kDev;
    else if (release_track == crosapi::kReleaseChannelCanary)
      os_channel = ChromeOSChannel::kCanary;
  }
  base::UmaHistogramEnumeration("ChromeOS.Lacros.OSChannel", os_channel);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void RecordMicroArchitectureStats() {
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  base::CPU::IntelMicroArchitecture arch = cpu.GetIntelMicroArchitecture();
  base::UmaHistogramEnumeration("Platform.IntelMaxMicroArchitecture", arch,
                                base::CPU::MAX_INTEL_MICRO_ARCHITECTURE);
#endif  // defined(ARCH_CPU_X86_FAMILY)
  base::UmaHistogramSparse("Platform.LogicalCpuCount",
                           base::SysInfo::NumberOfProcessors());
}

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
void RecordLinuxDistroSpecific(const std::string& version_string,
                               size_t parts,
                               const char* histogram_name) {
  base::Version version{version_string};
  if (!version.IsValid() || version.components().size() < parts)
    return;

  base::CheckedNumeric<int32_t> sample = 0;
  for (size_t i = 0; i < parts; i++) {
    sample *= 1000;
    sample += version.components()[i];
  }

  if (sample.IsValid())
    base::UmaHistogramSparse(histogram_name, sample.ValueOrDie());
}

void RecordLinuxDistro() {
  UMALinuxDistro distro_result = UMA_LINUX_DISTRO_UNKNOWN;

  std::vector<std::string> distro_tokens =
      base::SplitString(base::GetLinuxDistro(), base::kWhitespaceASCII,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (distro_tokens.size() > 0) {
    if (distro_tokens[0] == "Ubuntu") {
      // Format: Ubuntu YY.MM.P [LTS]
      // We are only concerned with release (YY.MM) not the patch (P).
      distro_result = UMA_LINUX_DISTRO_UBUNTU;
      if (distro_tokens.size() >= 2)
        RecordLinuxDistroSpecific(distro_tokens[1], 2, "Linux.Distro.Ubuntu");
    } else if (distro_tokens[0] == "openSUSE") {
      // Format: openSUSE Leap RR.R
      distro_result = UMA_LINUX_DISTRO_OPENSUSE_LEAP;
      if (distro_tokens.size() >= 3 && distro_tokens[1] == "Leap") {
        RecordLinuxDistroSpecific(distro_tokens[2], 2,
                                  "Linux.Distro.OpenSuseLeap");
      }
    } else if (distro_tokens[0] == "Debian") {
      // Format: Debian GNU/Linux R.P (<codename>)
      // We are only concerned with the release (R) not the patch (P).
      distro_result = UMA_LINUX_DISTRO_DEBIAN;
      if (distro_tokens.size() >= 3)
        RecordLinuxDistroSpecific(distro_tokens[2], 1, "Linux.Distro.Debian");
    } else if (distro_tokens[0] == "Fedora") {
      // Format: Fedora RR (<codename>)
      distro_result = UMA_LINUX_DISTRO_FEDORA;
      if (distro_tokens.size() >= 2)
        RecordLinuxDistroSpecific(distro_tokens[1], 1, "Linux.Distro.Fedora");
    } else if (distro_tokens[0] == "Arch") {
      // Format: Arch Linux
      distro_result = UMA_LINUX_DISTRO_ARCH;
    } else if (distro_tokens[0] == "CentOS") {
      // Format: CentOS [Linux] <version> (<codename>)
      distro_result = UMA_LINUX_DISTRO_CENTOS;
    } else if (distro_tokens[0] == "elementary") {
      // Format: elementary OS <release name>
      distro_result = UMA_LINUX_DISTRO_ELEMENTARY;
    } else if (distro_tokens.size() >= 2 && distro_tokens[1] == "Mint") {
      // Format: Linux Mint RR
      distro_result = UMA_LINUX_DISTRO_MINT;
      if (distro_tokens.size() >= 3)
        RecordLinuxDistroSpecific(distro_tokens[2], 1, "Linux.Distro.Mint");
    } else if (distro_tokens.size() >= 4 && distro_tokens[0] == "Red" &&
               distro_tokens[1] == "Hat" && distro_tokens[2] == "Enterprise" &&
               distro_tokens[3] == "Linux") {
      // Format: Red Hat Enterprise Linux <variant> R.P (<codename>)
      distro_result = UMA_LINUX_DISTRO_RHEL;
    } else if (distro_tokens.size() >= 3 && distro_tokens[0] == "SUSE" &&
               distro_tokens[1] == "Linux" &&
               distro_tokens[2] == "Enterprise") {
      // Format: SUSE Linux Enterprise <variant> RR
      distro_result = UMA_LINUX_DISTRO_SUSE_ENTERPRISE;
    }
  }

  base::UmaHistogramSparse("Linux.Distro2", distro_result);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

void RecordLinuxGlibcVersion() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(__GLIBC__) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  base::Version version(gnu_get_libc_version());

  UMALinuxGlibcVersion glibc_version_result = UMA_LINUX_GLIBC_NOT_PARSEABLE;
  if (version.IsValid() && version.components().size() == 2) {
    glibc_version_result = UMA_LINUX_GLIBC_UNKNOWN;
    uint32_t glibc_major_version = version.components()[0];
    uint32_t glibc_minor_version = version.components()[1];
    if (glibc_major_version == 2) {
      // A constant to translate glibc 2.x minor versions to their
      // equivalent UMALinuxGlibcVersion values.
      const int kGlibcMinorVersionTranslationOffset = 11 - UMA_LINUX_GLIBC_2_11;
      uint32_t translated_glibc_minor_version =
          glibc_minor_version - kGlibcMinorVersionTranslationOffset;
      if (translated_glibc_minor_version >= UMA_LINUX_GLIBC_2_11) {
        glibc_version_result =
            static_cast<UMALinuxGlibcVersion>(translated_glibc_minor_version);
      }
    }
  }
  base::UmaHistogramSparse("Linux.GlibcVersion", glibc_version_result);
#endif
}

void RecordTouchEventState() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string touch_enabled_switch =
      command_line.HasSwitch(switches::kTouchEventFeatureDetection)
          ? command_line.GetSwitchValueASCII(
                switches::kTouchEventFeatureDetection)
          : switches::kTouchEventFeatureDetectionAuto;

  UMATouchEventFeatureDetectionState state;
  if (touch_enabled_switch.empty() ||
      touch_enabled_switch == switches::kTouchEventFeatureDetectionEnabled) {
    state = UMA_TOUCH_EVENT_FEATURE_DETECTION_ENABLED;
  } else if (touch_enabled_switch ==
             switches::kTouchEventFeatureDetectionAuto) {
    state = (ui::GetTouchScreensAvailability() ==
             ui::TouchScreensAvailability::ENABLED)
                ? UMA_TOUCH_EVENT_FEATURE_DETECTION_AUTO_ENABLED
                : UMA_TOUCH_EVENT_FEATURE_DETECTION_AUTO_DISABLED;
  } else if (touch_enabled_switch ==
             switches::kTouchEventFeatureDetectionDisabled) {
    state = UMA_TOUCH_EVENT_FEATURE_DETECTION_DISABLED;
  } else {
    NOTREACHED();
    return;
  }

  base::UmaHistogramEnumeration("Touchscreen.TouchEventsEnabled", state,
                                UMA_TOUCH_EVENT_FEATURE_DETECTION_STATE_COUNT);
}

#if BUILDFLAG(IS_OZONE)

// Asynchronously records the touch event state when the ui::DeviceDataManager
// completes a device scan.
class AsynchronousTouchEventStateRecorder
    : public ui::InputDeviceEventObserver {
 public:
  AsynchronousTouchEventStateRecorder();

  AsynchronousTouchEventStateRecorder(
      const AsynchronousTouchEventStateRecorder&) = delete;
  AsynchronousTouchEventStateRecorder& operator=(
      const AsynchronousTouchEventStateRecorder&) = delete;

  ~AsynchronousTouchEventStateRecorder() override;

  // ui::InputDeviceEventObserver overrides.
  void OnDeviceListsComplete() override;
};

AsynchronousTouchEventStateRecorder::AsynchronousTouchEventStateRecorder() {
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}

AsynchronousTouchEventStateRecorder::~AsynchronousTouchEventStateRecorder() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

void AsynchronousTouchEventStateRecorder::OnDeviceListsComplete() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  RecordTouchEventState();
}

#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_WIN)
void RecordPinnedToTaskbarProcessError(bool error) {
  base::UmaHistogramBoolean("Windows.IsPinnedToTaskbar.ProcessError", error);
}

void OnShellHandlerConnectionError() {
  RecordPinnedToTaskbarProcessError(true);
}

// Record the UMA histogram when a response is received.
void OnIsPinnedToTaskbarResult(bool succeeded, bool is_pinned_to_taskbar) {
  RecordPinnedToTaskbarProcessError(false);

  // Used for histograms; do not reorder.
  enum Result { NOT_PINNED = 0, PINNED = 1, FAILURE = 2, NUM_RESULTS };

  Result result = FAILURE;
  if (succeeded)
    result = is_pinned_to_taskbar ? PINNED : NOT_PINNED;

  base::UmaHistogramEnumeration("Windows.IsPinnedToTaskbar", result,
                                NUM_RESULTS);

  // If Chrome is not pinned to taskbar, clear the recording that the installer
  // pinned Chrome to the taskbar, so that if the user pins Chrome back to the
  // taskbar, we don't count launches as coming from an installer-pinned
  // shortcut.  TODO(https://crbug.com/1353953): We currently only check if
  // Chrome is pinned to the taskbar 1 out every 100 launches, which makes this
  // less meaningful, so if keeping track of whether the installer pinned Chrome
  // to the taskbar is important, we need to deal with that.

  // Record whether or not the user unpinned an installer pin of Chrome. Records
  // true if the installer pinned Chrome, and it's not pinned on this startup,
  // false if the installer pinned Chrome, and it's still pinned.
  if (GetInstallerPinnedChromeToTaskbar().value_or(false)) {
    if (result == NOT_PINNED)
      SetInstallerPinnedChromeToTaskbar(false);
    if (result != FAILURE) {
      base::UmaHistogramBoolean("Windows.InstallerPinUnpinned",
                                result == NOT_PINNED);
    }
  }
}

// Records the pinned state of the current executable into a histogram. Should
// be called on a background thread, with low priority, to avoid slowing down
// startup.
void RecordIsPinnedToTaskbarHistogram() {
  shell_integration::win::GetIsPinnedToTaskbarState(
      base::BindOnce(&OnShellHandlerConnectionError),
      base::BindOnce(&OnIsPinnedToTaskbarResult));
}

// This registry key is not fully documented but there is information on it
// here:
// https://blogs.blackberry.com/en/2017/10/windows-10-parallel-loading-breakdown.
bool IsParallelDllLoadingEnabled() {
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path))
    return false;
  const wchar_t kIFEOKey[] =
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution "
      L"Options\\";
  std::wstring browser_process_key = kIFEOKey + exe_path.BaseName().value();

  base::win::RegKey key;
  if (ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE, browser_process_key.c_str(),
                                KEY_QUERY_VALUE))
    return true;

  const wchar_t kMaxLoaderThreads[] = L"MaxLoaderThreads";
  DWORD max_loader_threads = 0;
  if (ERROR_SUCCESS != key.ReadValueDW(kMaxLoaderThreads, &max_loader_threads))
    return true;

  // Note: If LoaderThreads is 0, it will be set to the default value of 4.
  return max_loader_threads != 1;
}

#endif  // BUILDFLAG(IS_WIN)

void RecordDisplayHDRStatus(const display::Display& display) {
  base::UmaHistogramBoolean("Hardware.Display.SupportsHDR",
                            display.color_spaces().SupportsHDR());
}

// Called on a background thread, with low priority to avoid slowing down
// startup with metrics that aren't trivial to compute.
void RecordStartupMetrics() {
#if BUILDFLAG(IS_WIN)
  const base::win::OSInfo& os_info = *base::win::OSInfo::GetInstance();
  int patch = os_info.version_number().patch;
  int build = os_info.version_number().build;
  int patch_level = 0;

  if (patch < 65536 && build < 65536)
    patch_level = MAKELONG(patch, build);
  DCHECK(patch_level) << "Windows version too high!";
  base::UmaHistogramSparse("Windows.PatchLevel", patch_level);

  int kernel32_patch = os_info.Kernel32VersionNumber().patch;
  int kernel32_build = os_info.Kernel32VersionNumber().build;
  int kernel32_patch_level = 0;
  if (kernel32_patch < 65536 && kernel32_build < 65536)
    kernel32_patch_level = MAKELONG(kernel32_patch, kernel32_build);
  DCHECK(kernel32_patch_level) << "Windows kernel32.dll version too high!";
  base::UmaHistogramSparse("Windows.PatchLevelKernel32", kernel32_patch_level);

  base::UmaHistogramBoolean("Windows.HasHighResolutionTimeTicks",
                            base::TimeTicks::IsHighResolution());

  // Determine whether parallel DLL loading is enabled for the browser process
  // executable. This is disabled by default on fresh Windows installations, but
  // the registry key that controls this might have been removed. Having the
  // parallel DLL loader enabled might affect both sandbox and early startup
  // behavior.
  base::UmaHistogramBoolean("Windows.ParallelDllLoadingEnabled",
                            IsParallelDllLoadingEnabled());
  crypto::MaybeMeasureTpmOperations();
#endif  // BUILDFLAG(IS_WIN)

  // Record whether Chrome is the default browser or not.
  // Disabled on Linux due to hanging browser tests, see crbug.com/1216328.
#if !BUILDFLAG(IS_LINUX)
  shell_integration::DefaultWebClientState default_state =
      shell_integration::GetDefaultBrowser();
  base::UmaHistogramEnumeration("DefaultBrowser.State", default_state,
                                shell_integration::NUM_DEFAULT_STATES);
#endif  // !BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  RecordChromeOSChannel();
#endif
}

}  // namespace

#if BUILDFLAG(IS_ANDROID)
bool IsBundleForMixedDeviceAccordingToVersionCode(
    const std::string& version_code) {
  // Primary bitness of the bundle is encoded in the last digit of the version
  // code. And the variant (package name) is encoded in the second to last.
  //
  // From build/util/android_chrome_version.py:
  //       'arm': {
  //          '32': 0,
  //          '32_64': 1,
  //          '64_32': 2,
  //          '64_32_high': 3,
  //          '64': 4,
  //      },
  //      'intel': {
  //          '32': 6,
  //          '32_64': 7,
  //          '64_32': 8,
  //          '64': 9,
  //      },
  //
  //      _PACKAGE_NAMES = {
  //          'CHROME': 0,
  //          'CHROME_MODERN': 10,
  //          'MONOCHROME': 20,
  //          'TRICHROME': 30,
  //          [...]

  if (version_code.length() < 2) {
    return false;
  }

  // '32' and '64' bundles go on 32bit-only and 64bit-only devices, so exclude
  // them.
  std::set<char> arch_codes_mixed = {'1', '2', '3', '7', '8'};
  char arch_code = version_code.back();

  // Only 'TRICHROME' supports 64-bit.
  constexpr char kTriChromeVariant = '3';
  char variant = version_code[version_code.length() - 2];

  return arch_codes_mixed.count(arch_code) > 0 && variant == kTriChromeVariant;
}
#endif  // BUILDFLAG(IS_ANDROID)

ChromeBrowserMainExtraPartsMetrics::ChromeBrowserMainExtraPartsMetrics()
    : display_count_(0) {}

ChromeBrowserMainExtraPartsMetrics::~ChromeBrowserMainExtraPartsMetrics() =
    default;

void ChromeBrowserMainExtraPartsMetrics::PreCreateThreads() {
#if !BUILDFLAG(IS_ANDROID)
  // Initialize the TabStatsTracker singleton instance. Must be initialized
  // before `responsiveness::Watcher`, which happens in
  // BrowserMainLoop::PreMainMessageLoopRun(), thus the decision to use
  // `PreCreateThreads`.
  // Only instantiate the tab stats tracker if a local state exists. This is
  // always the case for Chrome but not for the unittests.
  if (g_browser_process != nullptr &&
      g_browser_process->local_state() != nullptr) {
    metrics::TabStatsTracker::SetInstance(
        std::make_unique<metrics::TabStatsTracker>(
            g_browser_process->local_state()));
  }
#endif
}

void ChromeBrowserMainExtraPartsMetrics::PostCreateMainMessageLoop() {
#if !BUILDFLAG(IS_ANDROID)
  // Must be initialized before any child processes are spawned.
  process_monitor_ = std::make_unique<ProcessMonitor>();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserMainExtraPartsMetrics::PreProfileInit() {
  RecordMicroArchitectureStats();
}

void ChromeBrowserMainExtraPartsMetrics::PreBrowserStart() {
  flags_ui::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  about_flags::RecordUMAStatistics(&flags_storage, "Launch.FlagsAtStartup");

  // Log once here at browser start rather than at each renderer launch.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial("ClangPGO",
#if BUILDFLAG(CLANG_PGO)
#if BUILDFLAG(USE_THIN_LTO)
                                                            "EnabledWithThinLTO"
#else
                                                            "Enabled"
#endif
#else
                                                            "Disabled"
#endif
  );

  // Records whether or not the Segment heap is in use.
#if BUILDFLAG(IS_WIN)

  if (base::win::GetVersion() >= base::win::Version::WIN10_20H1) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial("WinSegmentHeap",
#if BUILDFLAG(ENABLE_SEGMENT_HEAP)
                                                              "OptedIn"
#else
                                                              "OptedOut"
#endif
    );
  } else {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial("WinSegmentHeap",
                                                              "NotSupported");
  }

  // Records whether or not CFG indirect call dispatch guards are present
  // or not.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial("WinCFG",
#if BUILDFLAG(WIN_ENABLE_CFG_GUARDS)
                                                            "Enabled"
#else
                                                            "Disabled"
#endif
  );

#endif  // BUILDFLAG(IS_WIN)

  // Register synthetic Finch trials proposed by PartitionAlloc.
  auto pa_trials = base::allocator::ProposeSyntheticFinchTrials();
  for (auto& trial : pa_trials) {
    auto [trial_name, group_name] = trial;
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(trial_name,
                                                              group_name);
  }

#if BUILDFLAG(IS_ANDROID)
  // Set up experiment for 64-bit Clank (incl. GWS visible IDs, so that the
  // groups are visible to Google servers).
  //
  // We are specifically interested in devices that meet all of these criteria:
  // 1) Devices with 4&6GB RAM, as we're launching the feature only for those
  //    (using (3.2;6.5) range to match RAM targeting in Play).
  // 2) Devices with only one Android profile (work versus personal), as having
  //    multiple profiles is a source of a population bias (so is having
  //    multiple users, but that bias is known to be small, and they're hard to
  //    filter out).
  // 3) Mixed 32-/64-bit devices, as non-mixed devices are forced to use
  //    a particular bitness, thus don't participate in the experiment.
  size_t ram_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  auto cpu_abi_bitness_support =
      metrics::AndroidMetricsHelper::GetInstance()->cpu_abi_bitness_support();
  bool is_device_of_interest =
      (3.2 * 1024 < ram_mb && ram_mb < 6.5 * 1024) &&
      (chrome::android::GetMultipleUserProfilesState() ==
       chrome::android::MultipleUserProfilesState::kSingleProfile) &&
      (cpu_abi_bitness_support == metrics::CpuAbiBitnessSupport::k32And64bit) &&
      IsBundleForMixedDeviceAccordingToVersionCode(
          base::android::BuildInfo::GetInstance()->package_version_code());
  if (is_device_of_interest) {
    uint32_t gws_experiment_id = 0;
    std::string trial_group;
    base::Version product_version(PRODUCT_VERSION);
#if defined(ARCH_CPU_64_BITS)
    trial_group = "64bit";
    if (product_version.IsValid()) {
      // For now, we only plan to run the experiment in Chrome 116 and 117, so
      // only send GWS IDs for those versions.
      switch (product_version.components()[0]) {
        case 116:
          gws_experiment_id = 3367343;
          break;
        case 117:
          gws_experiment_id = 3367345;
          break;
        default:
            // Leave 0-initialized.
            ;
      }
    }
#else   // defined(ARCH_CPU_64_BITS)
    trial_group = "32bit";
    if (product_version.IsValid()) {
      // For now, we only plan to run the experiment in Chrome 116 and 117, so
      // only send GWS IDs for those versions.
      switch (product_version.components()[0]) {
        case 116:
          gws_experiment_id = 3367342;
          break;
        case 117:
          gws_experiment_id = 3367344;
          break;
        default:
            // Leave 0-initialized.
            ;
      }
    }
#endif  // defined(ARCH_CPU_64_BITS)
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "BitnessForMidRangeRAM", trial_group,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "BitnessForMidRangeRAM_wVersion",
        std::string(PRODUCT_VERSION) + "_" + trial_group,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
    if (gws_experiment_id) {
      std::vector<std::string> ids = {base::NumberToString(gws_experiment_id)};
      variations::VariationsIdsProvider::GetInstance()->ForceVariationIds(ids,
                                                                          "");
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserMainExtraPartsMetrics::PostBrowserStart() {
  RecordMemoryMetricsAfterDelay();
  RecordLinuxGlibcVersion();

  constexpr base::TaskTraits kBestEffortTaskTraits = {
      base::MayBlock(), base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  base::ThreadPool::PostTask(FROM_HERE, kBestEffortTaskTraits,
                             base::BindOnce(&RecordLinuxDistro));
#endif

#if BUILDFLAG(IS_OZONE)
  // The touch event state for Ozone based event sub-systems are based on device
  // scans that happen asynchronously. So we may need to attach an observer to
  // wait until these scans complete.
  if (ui::DeviceDataManager::GetInstance()->AreDeviceListsComplete()) {
    RecordTouchEventState();
  } else {
    input_device_event_observer_ =
        std::make_unique<AsynchronousTouchEventStateRecorder>();
  }
#else
  RecordTouchEventState();
#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_MAC)
  RecordMacMetrics();
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  // RecordStartupMetrics calls into shell_integration::GetDefaultBrowser(),
  // which requires a COM thread on Windows.
  base::ThreadPool::CreateCOMSTATaskRunner(kBestEffortTaskTraits)
      ->PostTask(FROM_HERE, base::BindOnce(&RecordStartupMetrics));
#else
  base::ThreadPool::PostTask(FROM_HERE, kBestEffortTaskTraits,
                             base::BindOnce(&RecordStartupMetrics));
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
  // TODO(isherman): The delay below is currently needed to avoid (flakily)
  // breaking some tests, including all of the ProcessMemoryMetricsEmitterTest
  // tests. Figure out why there is a dependency and fix the tests.
  auto background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(kBestEffortTaskTraits);

  // The PinnedToTaskbar histogram is CPU intensive and can trigger a crashing
  // bug in Windows or in shell extensions so just sample the data to reduce the
  // cost.
  if (base::RandGenerator(100) == 0) {
    background_task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordIsPinnedToTaskbarHistogram),
        base::Seconds(45));
  }
#endif  // BUILDFLAG(IS_WIN)

  auto* screen = display::Screen::GetScreen();
  display_count_ = screen->GetNumDisplays();
  base::UmaHistogramCounts100("Hardware.Display.Count.OnStartup",
                              display_count_);

  for (const auto& display : screen->GetAllDisplays()) {
    RecordDisplayHDRStatus(display);
  }

  display_observer_.emplace(this);

#if !BUILDFLAG(IS_ANDROID)
  metrics::BeginFirstWebContentsProfiling();

  // Instantiate the power-related metrics reporters.

  // BatteryDischargeRateReporter reports the system-wide battery discharge
  // rate. It depends on the TabStatsTracker to determine the usage scenario,
  // and the BatteryStateSampler to determine the battery level.
  // The TabStatsTracker always exists (except during unit tests), while the
  // BatteryStateSampler only exists on platform where a BatteryLevelProvider
  // implementation exists.
  if (metrics::TabStatsTracker::GetInstance() &&
      base::BatteryStateSampler::Get()) {
    battery_discharge_reporter_ = std::make_unique<BatteryDischargeReporter>(
        base::BatteryStateSampler::Get());
  }

  // PowerMetricsReporter focus solely on Chrome-specific metrics that affect
  // power (CPU time, wake ups, etc.). Only instantiate it if |process_monitor_|
  // exists. This is always the case for Chrome but not for the unit tests.
  if (process_monitor_) {
    power_metrics_reporter_ =
        std::make_unique<PowerMetricsReporter>(process_monitor_.get());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX)
  pressure_metrics_reporter_ = std::make_unique<PressureMetricsReporter>();
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(
      power_metrics::SystemPowerMonitor::GetInstance());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  HandleEnableBenchmarkingCountdownAsync();
}

void ChromeBrowserMainExtraPartsMetrics::PreMainMessageLoopRun() {
  if (base::TimeTicks::IsConsistentAcrossProcesses()) {
    // Enable I/O jank monitoring for the browser process.
    base::EnableIOJankMonitoringForProcess(base::BindRepeating(
        [](int janky_intervals_per_minute, int total_janks_per_minute) {
          base::UmaHistogramCounts100(
              "Browser.Responsiveness.IOJankyIntervalsPerMinute",
              janky_intervals_per_minute);
          base::UmaHistogramCounts1000(
              "Browser.Responsiveness.IOJanksTotalPerMinute",
              total_janks_per_minute);
        }));
  }
}

void ChromeBrowserMainExtraPartsMetrics::PostMainMessageLoopRun() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_manager_observation_.Reset();
#endif
}

void ChromeBrowserMainExtraPartsMetrics::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kEnableBenchmarkingPrefId,
                                kEnableBenchmarkingCountdownDefault);
}

void ChromeBrowserMainExtraPartsMetrics::HandleEnableBenchmarkingCountdown(
    PrefService* pref_service,
    std::unique_ptr<flags_ui::FlagsStorage> storage,
    flags_ui::FlagAccess access) {
  std::set<std::string> flags = storage->GetFlags();

  // The implicit assumption here is that chrome://flags are stored in
  // flags_ui::PrefServiceFlagsStorage and the string matches the command line
  // flag. If the flag is not found (which should be the case for almost all
  // users) then this method short-circuits and does nothing.
  if (flags.find(variations::switches::kEnableBenchmarking) == flags.end()) {
    return;
  }

  int countdown = pref_service->GetInteger(kEnableBenchmarkingPrefId);
  countdown--;
  if (countdown <= 0) {
    // Clear the countdown pref.
    pref_service->ClearPref(kEnableBenchmarkingPrefId);

    // Clear the flag storage.
    flags.erase(variations::switches::kEnableBenchmarking);
    storage->SetFlags(std::move(flags));
  } else {
    pref_service->SetInteger(kEnableBenchmarkingPrefId, countdown);
  }
}

void ChromeBrowserMainExtraPartsMetrics::
    HandleEnableBenchmarkingCountdownAsync() {
  // On ChromeOS we must wait until post-login to be able to accurately assess
  // whether the enable-benchmarking flag has been enabled. This logic assumes
  // that it always runs pre-login.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
#else
  about_flags::GetStorage(/*profile=*/nullptr,
                          base::BindOnce(&HandleEnableBenchmarkingCountdown,
                                         g_browser_process->local_state()));
#endif
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayAdded(
    const display::Display& new_display) {
  EmitDisplaysChangedMetric();
  RecordDisplayHDRStatus(new_display);
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayRemoved(
    const display::Display& old_display) {
  EmitDisplaysChangedMetric();
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (changed_metrics & DisplayObserver::DISPLAY_METRIC_COLOR_SPACE) {
    RecordDisplayHDRStatus(display);
  }
}

void ChromeBrowserMainExtraPartsMetrics::EmitDisplaysChangedMetric() {
  int display_count = display::Screen::GetScreen()->GetNumDisplays();
  if (display_count != display_count_) {
    display_count_ = display_count;
    base::UmaHistogramCounts100("Hardware.Display.Count.OnChange",
                                display_count_);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeBrowserMainExtraPartsMetrics::OnProfileAdded(Profile* profile) {
  // This may be called with the login profile which is a side effect when
  // ash-chrome restarts during login. We only want to trigger the
  // HandleEnableBenchmarkingCountdown logic for the primary profile.

  if (!user_manager::UserManager::Get()->IsPrimaryUser(
          ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile))) {
    return;
  }

  about_flags::GetStorage(profile,
                          base::BindOnce(&HandleEnableBenchmarkingCountdown,
                                         g_browser_process->local_state()));
}
#endif

namespace chrome {

void AddMetricsExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsMetrics>());
}

}  // namespace chrome
