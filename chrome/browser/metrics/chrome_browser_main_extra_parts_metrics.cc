// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/mac/bluetooth_utility.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/metrics/first_web_contents_profiler.h"
#include "chrome/browser/metrics/tab_stats_tracker.h"
#include "chrome/browser/metrics/tab_usage_recorder.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID) && defined(__arm__)
#include <cpu-features.h>
#endif  // defined(OS_ANDROID) && defined(__arm__)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include <gnu/libc-version.h>

#include "base/linux_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(USE_OZONE) || defined(USE_X11)
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/input_device_manager.h"
#endif  // defined(USE_OZONE) || defined(USE_X11)

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/shell_integration_win.h"
#endif  // defined(OS_WIN)

namespace {

// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum UMALinuxDistro {
  UMA_LINUX_DISTRO_UNKNOWN = 0,
  UMA_LINUX_DISTRO_UBUNTU_OTHER_DEPRECATED = 1,
  UMA_LINUX_DISTRO_UBUNTU_14_04_DEPRECATED = 2,
  UMA_LINUX_DISTRO_UBUNTU_16_04_DEPRECATED = 3,
  UMA_LINUX_DISTRO_UBUNTU_16_10_DEPRECATED = 4,
  UMA_LINUX_DISTRO_UBUNTU_17_04_DEPRECATED = 5,
  UMA_LINUX_DISTRO_DEBIAN_OTHER_DEPRECATED = 6,
  UMA_LINUX_DISTRO_DEBIAN_8_DEPRECATED = 7,
  UMA_LINUX_DISTRO_OPENSUSE_OTHER_DEPRECATED = 8,
  UMA_LINUX_DISTRO_OPENSUSE_LEAP_42_2_DEPRECATED = 9,
  UMA_LINUX_DISTRO_FEDORA_OTHER_DEPRECATED = 10,
  UMA_LINUX_DISTRO_FEDORA_24_DEPRECATED = 11,
  UMA_LINUX_DISTRO_FEDORA_25_DEPRECATED = 12,
  UMA_LINUX_DISTRO_FEDORA_26_DEPRECATED = 13,
  UMA_LINUX_DISTRO_DEBIAN_9_DEPRECATED = 14,
  UMA_LINUX_DISTRO_ARCH = 15,
  UMA_LINUX_DISTRO_CENTOS = 16,
  UMA_LINUX_DISTRO_ELEMENTARY = 17,
  UMA_LINUX_DISTRO_MINT = 18,
  UMA_LINUX_DISTRO_RHEL = 19,
  UMA_LINUX_DISTRO_SUSE_ENTERPRISE = 20,
  // Debian
  UMA_LINUX_DISTRO_DEBIAN_OTHER = 50,
  UMA_LINUX_DISTRO_DEBIAN_8 = 51,
  UMA_LINUX_DISTRO_DEBIAN_9 = 52,
  UMA_LINUX_DISTRO_DEBIAN_10 = 53,
  // Fedora
  UMA_LINUX_DISTRO_FEDORA_OTHER = 100,
  UMA_LINUX_DISTRO_FEDORA_24 = 101,
  UMA_LINUX_DISTRO_FEDORA_25 = 102,
  UMA_LINUX_DISTRO_FEDORA_26 = 103,
  UMA_LINUX_DISTRO_FEDORA_27 = 104,
  UMA_LINUX_DISTRO_FEDORA_28 = 105,
  // openSUSE
  UMA_LINUX_DISTRO_OPENSUSE_OTHER = 150,
  UMA_LINUX_DISTRO_OPENSUSE_LEAP_42_2 = 151,
  UMA_LINUX_DISTRO_OPENSUSE_LEAP_42_3 = 152,
  UMA_LINUX_DISTRO_OPENSUSE_LEAP_15_0 = 153,
  UMA_LINUX_DISTRO_OPENSUSE_LEAP_15_1 = 154,
  UMA_LINUX_DISTRO_OPENSUSE_LEAP_15_2 = 155,
  // Ubuntu
  UMA_LINUX_DISTRO_UBUNTU_OTHER = 200,
  UMA_LINUX_DISTRO_UBUNTU_14_04 = 201,
  UMA_LINUX_DISTRO_UBUNTU_16_04 = 202,
  UMA_LINUX_DISTRO_UBUNTU_16_10 = 203,
  UMA_LINUX_DISTRO_UBUNTU_17_04 = 204,
  UMA_LINUX_DISTRO_UBUNTU_17_10 = 205,
  UMA_LINUX_DISTRO_UBUNTU_18_04 = 206,
  UMA_LINUX_DISTRO_UBUNTU_18_10 = 207,
  // Note: Add new distros to the list above this line, and update LinuxDistro
  // in tools/metrics/histograms/enums.xml accordingly.
  UMA_LINUX_DISTRO_MAX
};

enum UMALinuxGlibcVersion {
  UMA_LINUX_GLIBC_NOT_PARSEABLE,
  UMA_LINUX_GLIBC_UNKNOWN,
  UMA_LINUX_GLIBC_2_11,
  // To log newer versions, just update tools/metrics/histograms/histograms.xml.
};

enum UMALinuxWindowManager {
  UMA_LINUX_WINDOW_MANAGER_OTHER,
  UMA_LINUX_WINDOW_MANAGER_BLACKBOX,
  UMA_LINUX_WINDOW_MANAGER_CHROME_OS,  // Deprecated.
  UMA_LINUX_WINDOW_MANAGER_COMPIZ,
  UMA_LINUX_WINDOW_MANAGER_ENLIGHTENMENT,
  UMA_LINUX_WINDOW_MANAGER_ICE_WM,
  UMA_LINUX_WINDOW_MANAGER_KWIN,
  UMA_LINUX_WINDOW_MANAGER_METACITY,
  UMA_LINUX_WINDOW_MANAGER_MUFFIN,
  UMA_LINUX_WINDOW_MANAGER_MUTTER,
  UMA_LINUX_WINDOW_MANAGER_OPENBOX,
  UMA_LINUX_WINDOW_MANAGER_XFWM4,
  UMA_LINUX_WINDOW_MANAGER_AWESOME,
  UMA_LINUX_WINDOW_MANAGER_I3,
  UMA_LINUX_WINDOW_MANAGER_ION3,
  UMA_LINUX_WINDOW_MANAGER_MATCHBOX,
  UMA_LINUX_WINDOW_MANAGER_NOTION,
  UMA_LINUX_WINDOW_MANAGER_QTILE,
  UMA_LINUX_WINDOW_MANAGER_RATPOISON,
  UMA_LINUX_WINDOW_MANAGER_STUMPWM,
  UMA_LINUX_WINDOW_MANAGER_WMII,
  UMA_LINUX_WINDOW_MANAGER_FLUXBOX,
  UMA_LINUX_WINDOW_MANAGER_XMONAD,
  UMA_LINUX_WINDOW_MANAGER_UNNAMED,
  // NOTE: Append new window managers to the list above this line (i.e. don't
  // renumber) and update LinuxWindowManagerName in
  // tools/metrics/histograms/histograms.xml accordingly.
  UMA_LINUX_WINDOW_MANAGER_COUNT
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

#if defined(OS_ANDROID) && defined(__arm__)
enum UMAAndroidArmFpu {
  UMA_ANDROID_ARM_FPU_VFPV3_D16,  // The ARM CPU only supports vfpv3-d16.
  UMA_ANDROID_ARM_FPU_NEON,       // The Arm CPU supports NEON.
  UMA_ANDROID_ARM_FPU_COUNT
};
#endif  // defined(OS_ANDROID) && defined(__arm__)

void RecordMicroArchitectureStats() {
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  base::CPU::IntelMicroArchitecture arch = cpu.GetIntelMicroArchitecture();
  UMA_HISTOGRAM_ENUMERATION("Platform.IntelMaxMicroArchitecture", arch,
                            base::CPU::MAX_INTEL_MICRO_ARCHITECTURE);
#endif  // defined(ARCH_CPU_X86_FAMILY)
#if defined(OS_ANDROID) && defined(__arm__)
  // Detect NEON support.
  // TODO(fdegans): Remove once non-NEON support has been removed.
  if (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) {
    UMA_HISTOGRAM_ENUMERATION("Android.ArmFpu",
                              UMA_ANDROID_ARM_FPU_NEON,
                              UMA_ANDROID_ARM_FPU_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Android.ArmFpu",
                              UMA_ANDROID_ARM_FPU_VFPV3_D16,
                              UMA_ANDROID_ARM_FPU_COUNT);
  }
#endif  // defined(OS_ANDROID) && defined(__arm__)
  base::UmaHistogramSparse("Platform.LogicalCpuCount",
                           base::SysInfo::NumberOfProcessors());
}

// Called on a background thread, with low priority to avoid slowing down
// startup with metrics that aren't trivial to compute.
void RecordStartupMetrics() {
#if defined(OS_WIN)
  const base::win::OSInfo& os_info = *base::win::OSInfo::GetInstance();
  UMA_HISTOGRAM_ENUMERATION("Windows.GetVersionExVersion", os_info.version(),
                            base::win::VERSION_WIN_LAST);
  UMA_HISTOGRAM_ENUMERATION("Windows.Kernel32Version",
                            os_info.Kernel32Version(),
                            base::win::VERSION_WIN_LAST);
  UMA_HISTOGRAM_BOOLEAN("Windows.InCompatibilityMode",
                        os_info.version() != os_info.Kernel32Version());

  UMA_HISTOGRAM_BOOLEAN("Windows.HasHighResolutionTimeTicks",
                        base::TimeTicks::IsHighResolution());
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
  bluetooth_utility::BluetoothAvailability availability =
      bluetooth_utility::GetBluetoothAvailability();
  UMA_HISTOGRAM_ENUMERATION("OSX.BluetoothAvailability",
                            availability,
                            bluetooth_utility::BLUETOOTH_AVAILABILITY_COUNT);
#endif  // defined(OS_MACOSX)

  // Record whether Chrome is the default browser or not.
  shell_integration::DefaultWebClientState default_state =
      shell_integration::GetDefaultBrowser();
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.State", default_state,
                            shell_integration::NUM_DEFAULT_STATES);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void RecordLinuxDistro() {
  UMALinuxDistro distro_result = UMA_LINUX_DISTRO_UNKNOWN;

  std::vector<std::string> distro_tokens =
      base::SplitString(base::GetLinuxDistro(), base::kWhitespaceASCII,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (distro_tokens.size() > 0) {
    if (distro_tokens[0] == "Ubuntu") {
      // Format: Ubuntu YY.MM.P [LTS]
      // We are only concerned with release (YY.MM) not the patch (P).
      distro_result = UMA_LINUX_DISTRO_UBUNTU_OTHER;
      if (distro_tokens.size() >= 2) {
        base::Version version(distro_tokens[1]);
        if (version.IsValid()) {
          if (version.CompareToWildcardString("14.04.*") == 0) {
            distro_result = UMA_LINUX_DISTRO_UBUNTU_14_04;
          } else if (version.CompareToWildcardString("16.04.*") == 0) {
            distro_result = UMA_LINUX_DISTRO_UBUNTU_16_04;
          } else if (version.CompareToWildcardString("16.10.*") == 0) {
            distro_result = UMA_LINUX_DISTRO_UBUNTU_16_10;
          } else if (version.CompareToWildcardString("17.04.*") == 0) {
            distro_result = UMA_LINUX_DISTRO_UBUNTU_17_04;
          } else if (version.CompareToWildcardString("17.10.*") == 0) {
            distro_result = UMA_LINUX_DISTRO_UBUNTU_17_10;
          } else if (version.CompareToWildcardString("18.04.*") == 0) {
            distro_result = UMA_LINUX_DISTRO_UBUNTU_18_04;
          } else if (version.CompareToWildcardString("18.10.*") == 0) {
            distro_result = UMA_LINUX_DISTRO_UBUNTU_18_10;
          }
        }
      }
    } else if (distro_tokens[0] == "openSUSE") {
      // Format: openSUSE Leap RR.R
      distro_result = UMA_LINUX_DISTRO_OPENSUSE_OTHER;
      if (distro_tokens.size() >= 3 && distro_tokens[1] == "Leap") {
        if (distro_tokens[2] == "42.2") {
          distro_result = UMA_LINUX_DISTRO_OPENSUSE_LEAP_42_2;
        } else if (distro_tokens[2] == "42.3") {
          distro_result = UMA_LINUX_DISTRO_OPENSUSE_LEAP_42_3;
        } else if (distro_tokens[2] == "15.0") {
          distro_result = UMA_LINUX_DISTRO_OPENSUSE_LEAP_15_0;
        } else if (distro_tokens[2] == "15.1") {
          distro_result = UMA_LINUX_DISTRO_OPENSUSE_LEAP_15_1;
        } else if (distro_tokens[2] == "15.2") {
          distro_result = UMA_LINUX_DISTRO_OPENSUSE_LEAP_15_2;
        }
      }
    } else if (distro_tokens[0] == "Debian") {
      // Format: Debian GNU/Linux R.P (<codename>)
      // We are only concerned with the release (R) not the patch (P).
      distro_result = UMA_LINUX_DISTRO_DEBIAN_OTHER;
      if (distro_tokens.size() >= 3) {
        base::Version version(distro_tokens[2]);
        if (version.IsValid()) {
          if (version.CompareToWildcardString("8.*")) {
            distro_result = UMA_LINUX_DISTRO_DEBIAN_8;
          } else if (version.CompareToWildcardString("9.*")) {
            distro_result = UMA_LINUX_DISTRO_DEBIAN_9;
          } else if (version.CompareToWildcardString("10.*")) {
            distro_result = UMA_LINUX_DISTRO_DEBIAN_10;
          }
        }
      }
    } else if (distro_tokens[0] == "Fedora") {
      // Format: Fedora release RR (<codename>)
      distro_result = UMA_LINUX_DISTRO_FEDORA_OTHER;
      if (distro_tokens.size() >= 3) {
        if (distro_tokens[2] == "24") {
          distro_result = UMA_LINUX_DISTRO_FEDORA_24;
        } else if (distro_tokens[2] == "25") {
          distro_result = UMA_LINUX_DISTRO_FEDORA_25;
        } else if (distro_tokens[2] == "26") {
          distro_result = UMA_LINUX_DISTRO_FEDORA_26;
        } else if (distro_tokens[2] == "27") {
          distro_result = UMA_LINUX_DISTRO_FEDORA_27;
        } else if (distro_tokens[2] == "28") {
          distro_result = UMA_LINUX_DISTRO_FEDORA_28;
        }
      }
    } else if (distro_tokens[0] == "Arch") {
      // Format: Arch Linux
      distro_result = UMA_LINUX_DISTRO_ARCH;
    } else if (distro_tokens[0] == "CentOS") {
      // Format: CentOS [Linux] release <version> (<codename>)
      distro_result = UMA_LINUX_DISTRO_CENTOS;
    } else if (distro_tokens[0] == "elementary") {
      // Format: elementary OS <release name>
      distro_result = UMA_LINUX_DISTRO_ELEMENTARY;
    } else if (distro_tokens.size() >= 2 && distro_tokens[1] == "Mint") {
      // Format: Linux Mint RR <codename>
      distro_result = UMA_LINUX_DISTRO_MINT;
    } else if (distro_tokens.size() >= 4 && distro_tokens[0] == "Red" &&
               distro_tokens[1] == "Hat" && distro_tokens[2] == "Enterprise" &&
               distro_tokens[3] == "Linux") {
      // Format: Red Hat Enterprise Linux <variant> [release] R.P (<codename>)
      distro_result = UMA_LINUX_DISTRO_RHEL;
    } else if (distro_tokens.size() >= 3 && distro_tokens[0] == "SUSE" &&
               distro_tokens[1] == "Linux" &&
               distro_tokens[2] == "Enterprise") {
      // Format: SUSE Linux Enterprise <variant> RR (<platform>)
      distro_result = UMA_LINUX_DISTRO_SUSE_ENTERPRISE;
    }
  }

  base::UmaHistogramSparse("Linux.Distro", distro_result);
}
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

void RecordLinuxGlibcVersion() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
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

#if defined(USE_X11)
UMALinuxWindowManager GetLinuxWindowManager() {
  switch (ui::GuessWindowManager()) {
    case ui::WM_OTHER:
      return UMA_LINUX_WINDOW_MANAGER_OTHER;
    case ui::WM_UNNAMED:
      return UMA_LINUX_WINDOW_MANAGER_UNNAMED;
    case ui::WM_AWESOME:
      return UMA_LINUX_WINDOW_MANAGER_AWESOME;
    case ui::WM_BLACKBOX:
      return UMA_LINUX_WINDOW_MANAGER_BLACKBOX;
    case ui::WM_COMPIZ:
      return UMA_LINUX_WINDOW_MANAGER_COMPIZ;
    case ui::WM_ENLIGHTENMENT:
      return UMA_LINUX_WINDOW_MANAGER_ENLIGHTENMENT;
    case ui::WM_FLUXBOX:
      return UMA_LINUX_WINDOW_MANAGER_FLUXBOX;
    case ui::WM_I3:
      return UMA_LINUX_WINDOW_MANAGER_I3;
    case ui::WM_ICE_WM:
      return UMA_LINUX_WINDOW_MANAGER_ICE_WM;
    case ui::WM_ION3:
      return UMA_LINUX_WINDOW_MANAGER_ION3;
    case ui::WM_KWIN:
      return UMA_LINUX_WINDOW_MANAGER_KWIN;
    case ui::WM_MATCHBOX:
      return UMA_LINUX_WINDOW_MANAGER_MATCHBOX;
    case ui::WM_METACITY:
      return UMA_LINUX_WINDOW_MANAGER_METACITY;
    case ui::WM_MUFFIN:
      return UMA_LINUX_WINDOW_MANAGER_MUFFIN;
    case ui::WM_MUTTER:
      return UMA_LINUX_WINDOW_MANAGER_MUTTER;
    case ui::WM_NOTION:
      return UMA_LINUX_WINDOW_MANAGER_NOTION;
    case ui::WM_OPENBOX:
      return UMA_LINUX_WINDOW_MANAGER_OPENBOX;
    case ui::WM_QTILE:
      return UMA_LINUX_WINDOW_MANAGER_QTILE;
    case ui::WM_RATPOISON:
      return UMA_LINUX_WINDOW_MANAGER_RATPOISON;
    case ui::WM_STUMPWM:
      return UMA_LINUX_WINDOW_MANAGER_STUMPWM;
    case ui::WM_WMII:
      return UMA_LINUX_WINDOW_MANAGER_WMII;
    case ui::WM_XFWM4:
      return UMA_LINUX_WINDOW_MANAGER_XFWM4;
    case ui::WM_XMONAD:
      return UMA_LINUX_WINDOW_MANAGER_XMONAD;
  }
  NOTREACHED();
  return UMA_LINUX_WINDOW_MANAGER_OTHER;
}
#endif

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

  UMA_HISTOGRAM_ENUMERATION("Touchscreen.TouchEventsEnabled", state,
                            UMA_TOUCH_EVENT_FEATURE_DETECTION_STATE_COUNT);
}

#if defined(USE_OZONE) || defined(USE_X11)

// Asynchronously records the touch event state when the ui::DeviceDataManager
// completes a device scan.
class AsynchronousTouchEventStateRecorder
    : public ui::InputDeviceEventObserver {
 public:
  AsynchronousTouchEventStateRecorder();
  ~AsynchronousTouchEventStateRecorder() override;

  // ui::InputDeviceEventObserver overrides.
  void OnDeviceListsComplete() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsynchronousTouchEventStateRecorder);
};

AsynchronousTouchEventStateRecorder::AsynchronousTouchEventStateRecorder() {
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

AsynchronousTouchEventStateRecorder::~AsynchronousTouchEventStateRecorder() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
}

void AsynchronousTouchEventStateRecorder::OnDeviceListsComplete() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  RecordTouchEventState();
}

#endif  // defined(USE_OZONE) || defined(USE_X11)

#if defined(OS_WIN)
void RecordPinnedToTaskbarProcessError(bool error) {
  UMA_HISTOGRAM_BOOLEAN("Windows.IsPinnedToTaskbar.ProcessError", error);
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
  UMA_HISTOGRAM_ENUMERATION("Windows.IsPinnedToTaskbar", result, NUM_RESULTS);
}

// Records the pinned state of the current executable into a histogram. Should
// be called on a background thread, with low priority, to avoid slowing down
// startup.
void RecordIsPinnedToTaskbarHistogram(
    std::unique_ptr<service_manager::Connector> connector) {
  shell_integration::win::GetIsPinnedToTaskbarState(
      std::move(connector), base::Bind(&OnShellHandlerConnectionError),
      base::Bind(&OnIsPinnedToTaskbarResult));
}

void RecordVrStartupHistograms() {
  vr::XRRuntimeManager::RecordVrStartupHistograms();
}
#endif  // defined(OS_WIN)

}  // namespace

ChromeBrowserMainExtraPartsMetrics::ChromeBrowserMainExtraPartsMetrics()
    : display_count_(0), is_screen_observer_(false) {
}

ChromeBrowserMainExtraPartsMetrics::~ChromeBrowserMainExtraPartsMetrics() {
  if (is_screen_observer_)
    display::Screen::GetScreen()->RemoveObserver(this);
}

void ChromeBrowserMainExtraPartsMetrics::PreProfileInit() {
  RecordMicroArchitectureStats();
}

void ChromeBrowserMainExtraPartsMetrics::PreBrowserStart() {
  flags_ui::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  about_flags::RecordUMAStatistics(&flags_storage);

#if defined(OS_WIN)
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial("ChromeWinClang",
#if defined(__clang__)
                                                            "Enabled"
#else
                                                            "Disabled"
#endif
                                                            );
#endif
}

void ChromeBrowserMainExtraPartsMetrics::PostBrowserStart() {
  RecordLinuxGlibcVersion();
#if defined(USE_X11)
  UMA_HISTOGRAM_ENUMERATION("Linux.WindowManager", GetLinuxWindowManager(),
                            UMA_LINUX_WINDOW_MANAGER_COUNT);
#endif

  constexpr base::TaskTraits background_task_traits = {
      base::MayBlock(), base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  base::PostTaskWithTraits(FROM_HERE, background_task_traits,
                           base::BindOnce(&RecordLinuxDistro));
#endif

#if defined(USE_OZONE) || defined(USE_X11)
  // The touch event state for X11 and Ozone based event sub-systems are based
  // on device scans that happen asynchronously. So we may need to attach an
  // observer to wait until these scans complete.
  if (ui::InputDeviceManager::GetInstance()->AreDeviceListsComplete()) {
    RecordTouchEventState();
  } else {
    input_device_event_observer_.reset(
        new AsynchronousTouchEventStateRecorder());
  }
#else
  RecordTouchEventState();
#endif  // defined(USE_OZONE) || defined(USE_X11)

#if defined(OS_MACOSX)
  RecordMacMetrics();
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
  // RecordStartupMetrics calls into shell_integration::GetDefaultBrowser(),
  // which requires a COM thread on Windows.
  base::CreateCOMSTATaskRunnerWithTraits(background_task_traits)
      ->PostTask(FROM_HERE, base::BindOnce(&RecordStartupMetrics));
#else
  base::PostTaskWithTraits(FROM_HERE, background_task_traits,
                           base::BindOnce(&RecordStartupMetrics));
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
  // TODO(isherman): The delay below is currently needed to avoid (flakily)
  // breaking some tests, including all of the ProcessMemoryMetricsEmitterTest
  // tests. Figure out why there is a dependency and fix the tests.
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();

  auto background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(background_task_traits);

  background_task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RecordIsPinnedToTaskbarHistogram, connector->Clone()),
      base::TimeDelta::FromSeconds(45));

  // TODO(billorr): This should eventually be done on all platforms that support
  // VR.
  background_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RecordVrStartupHistograms),
      base::TimeDelta::FromSeconds(45));
#endif  // defined(OS_WIN)

  display_count_ = display::Screen::GetScreen()->GetNumDisplays();
  UMA_HISTOGRAM_COUNTS_100("Hardware.Display.Count.OnStartup", display_count_);
  display::Screen::GetScreen()->AddObserver(this);
  is_screen_observer_ = true;

#if !defined(OS_ANDROID)
  metrics::BeginFirstWebContentsProfiling();
  metrics::TabUsageRecorder::InitializeIfNeeded();
  // Only instantiate the tab stats tracker if a local state exists. This is
  // always the case for Chrome but not for the unittests.
  if (g_browser_process != nullptr &&
      g_browser_process->local_state() != nullptr) {
    metrics::TabStatsTracker::SetInstance(
        std::make_unique<metrics::TabStatsTracker>(
            g_browser_process->local_state()));
  }
#endif  // !defined(OS_ANDROID)
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayAdded(
    const display::Display& new_display) {
  EmitDisplaysChangedMetric();
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayRemoved(
    const display::Display& old_display) {
  EmitDisplaysChangedMetric();
}

void ChromeBrowserMainExtraPartsMetrics::EmitDisplaysChangedMetric() {
  int display_count = display::Screen::GetScreen()->GetNumDisplays();
  if (display_count != display_count_) {
    display_count_ = display_count;
    UMA_HISTOGRAM_COUNTS_100("Hardware.Display.Count.OnChange", display_count_);
  }
}

namespace chrome {

void AddMetricsExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsMetrics());
}

}  // namespace chrome
