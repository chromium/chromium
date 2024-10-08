// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/windows_version.h"

#include <windows.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/registry.h"
#include "build/build_config.h"

#if !defined(__clang__) && _MSC_FULL_VER < 191125507
#error VS 2017 Update 3.2 or higher is required
#endif

#if !defined(NTDDI_WIN10_NI)
#error Windows 10.0.22621.0 SDK or higher required.
#endif

namespace base {
namespace win {

namespace {

// The values under the CurrentVersion registry hive are mirrored under
// the corresponding Wow6432 hive.
constexpr wchar_t kRegKeyWindowsNTCurrentVersion[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

// Returns the "UBR" (Windows 10 patch number) and "DisplayVersion" (or
// "ReleaseId" on earlier versions) (Windows 10 release number) from registry.
// "UBR" is an undocumented value and will be 0 if the value was not found.
// "ReleaseId" will be an empty string if neither new nor old values are found.
std::pair<int, std::string> GetVersionData() {
  DWORD ubr = 0;
  std::wstring release_id;
  RegKey key;

  if (key.Open(HKEY_LOCAL_MACHINE, kRegKeyWindowsNTCurrentVersion,
               KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    key.ReadValueDW(L"UBR", &ubr);
    // "DisplayVersion" has been introduced in Windows 10 2009
    // when naming changed to mixed letters and numbers.
    key.ReadValue(L"DisplayVersion", &release_id);
    // Use discontinued "ReleaseId" instead, if the former is unavailable.
    if (release_id.empty())
      key.ReadValue(L"ReleaseId", &release_id);
  }

  return std::make_pair(static_cast<int>(ubr), WideToUTF8(release_id));
}

const _SYSTEM_INFO& GetSystemInfoStorage() {
  static const _SYSTEM_INFO system_info = [] {
    _SYSTEM_INFO info = {};
    ::GetNativeSystemInfo(&info);
    return info;
  }();
  return system_info;
}

}  // namespace

// static
OSInfo** OSInfo::GetInstanceStorage() {
  // Note: we don't use the Singleton class because it depends on AtExitManager,
  // and it's convenient for other modules to use this class without it.
  static OSInfo* info = [] {
    _OSVERSIONINFOEXW version_info = {sizeof(version_info)};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // GetVersionEx() is deprecated, and the suggested replacement are
    // the IsWindows*OrGreater() functions in VersionHelpers.h. We can't
    // use that because:
    // - For Windows 10, there's IsWindows10OrGreater(), but nothing more
    //   granular. We need to be able to detect different Windows 10 releases
    //   since they sometimes change behavior in ways that matter.
    // - There is no IsWindows11OrGreater() function yet.
    ::GetVersionEx(reinterpret_cast<_OSVERSIONINFOW*>(&version_info));
#pragma clang diagnostic pop

    DWORD os_type = 0;
    ::GetProductInfo(version_info.dwMajorVersion, version_info.dwMinorVersion,
                     0, 0, &os_type);

    return new OSInfo(version_info, GetSystemInfoStorage(), os_type);
  }();

  return &info;
}

// static
OSInfo* OSInfo::GetInstance() {
  return *GetInstanceStorage();
}

// static
OSInfo::WindowsArchitecture OSInfo::GetArchitecture() {
  switch (GetSystemInfoStorage().wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL:
      return X86_ARCHITECTURE;
    case PROCESSOR_ARCHITECTURE_AMD64:
      return X64_ARCHITECTURE;
    case PROCESSOR_ARCHITECTURE_IA64:
      return IA64_ARCHITECTURE;
    case PROCESSOR_ARCHITECTURE_ARM64:
      return ARM64_ARCHITECTURE;
    default:
      return OTHER_ARCHITECTURE;
  }
}

// Returns true if this is an x86/x64 process running on ARM64 through
// emulation.
// static
bool OSInfo::IsRunningEmulatedOnArm64() {
#if defined(ARCH_CPU_ARM64)
  // If we're running native ARM64 then we aren't running emulated.
  return false;
#else
  using IsWow64Process2Function = decltype(&IsWow64Process2);

  IsWow64Process2Function is_wow64_process2 =
      reinterpret_cast<IsWow64Process2Function>(::GetProcAddress(
          ::GetModuleHandleA("kernel32.dll"), "IsWow64Process2"));
  if (!is_wow64_process2) {
    return false;
  }
  USHORT process_machine;
  USHORT native_machine;
  bool retval = is_wow64_process2(::GetCurrentProcess(), &process_machine,
                                  &native_machine);
  if (!retval) {
    return false;
  }
  if (native_machine == IMAGE_FILE_MACHINE_ARM64) {
    return true;
  }
  return false;
#endif
}

OSInfo::OSInfo(const _OSVERSIONINFOEXW& version_info,
               const _SYSTEM_INFO& system_info,
               DWORD os_type)
    : version_(Version::PRE_XP),
      wow_process_machine_(WowProcessMachine::kUnknown),
      wow_native_machine_(WowNativeMachine::kUnknown),
      os_type_(os_type) {
  version_number_.major = version_info.dwMajorVersion;
  version_number_.minor = version_info.dwMinorVersion;
  version_number_.build = version_info.dwBuildNumber;
  std::tie(version_number_.patch, release_id_) = GetVersionData();
  version_ = MajorMinorBuildToVersion(
      version_number_.major, version_number_.minor, version_number_.build);
  InitializeWowStatusValuesForProcess(GetCurrentProcess());
  service_pack_.major = version_info.wServicePackMajor;
  service_pack_.minor = version_info.wServicePackMinor;
  service_pack_str_ = WideToUTF8(version_info.szCSDVersion);

  processors_ = static_cast<int>(system_info.dwNumberOfProcessors);
  allocation_granularity_ = system_info.dwAllocationGranularity;

  if (version_info.dwMajorVersion == 6 || version_info.dwMajorVersion == 10) {
    // Only present on Vista+.
    switch (os_type) {
      case PRODUCT_CLUSTER_SERVER:
      case PRODUCT_DATACENTER_SERVER:
      case PRODUCT_DATACENTER_SERVER_CORE:
      case PRODUCT_ENTERPRISE_SERVER:
      case PRODUCT_ENTERPRISE_SERVER_CORE:
      case PRODUCT_ENTERPRISE_SERVER_IA64:
      case PRODUCT_SMALLBUSINESS_SERVER:
      case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
      case PRODUCT_STANDARD_SERVER:
      case PRODUCT_STANDARD_SERVER_CORE:
      case PRODUCT_WEB_SERVER:
        version_type_ = SUITE_SERVER;
        break;
      case PRODUCT_PROFESSIONAL:
      case PRODUCT_ULTIMATE:
        version_type_ = SUITE_PROFESSIONAL;
        break;
      case PRODUCT_ENTERPRISE:
      case PRODUCT_ENTERPRISE_E:
      case PRODUCT_ENTERPRISE_EVALUATION:
      case PRODUCT_ENTERPRISE_N:
      case PRODUCT_ENTERPRISE_N_EVALUATION:
      case PRODUCT_ENTERPRISE_S:
      case PRODUCT_ENTERPRISE_S_EVALUATION:
      case PRODUCT_ENTERPRISE_S_N:
      case PRODUCT_ENTERPRISE_S_N_EVALUATION:
      case PRODUCT_ENTERPRISE_SUBSCRIPTION:
      case PRODUCT_ENTERPRISE_SUBSCRIPTION_N:
      case PRODUCT_BUSINESS:
      case PRODUCT_BUSINESS_N:
      case PRODUCT_IOTENTERPRISE:
      case PRODUCT_IOTENTERPRISES:
        version_type_ = SUITE_ENTERPRISE;
        break;
      case PRODUCT_PRO_FOR_EDUCATION:
      case PRODUCT_PRO_FOR_EDUCATION_N:
        version_type_ = SUITE_EDUCATION_PRO;
        break;
      case PRODUCT_EDUCATION:
      case PRODUCT_EDUCATION_N:
        version_type_ = SUITE_EDUCATION;
        break;
      case PRODUCT_HOME_BASIC:
      case PRODUCT_HOME_PREMIUM:
      case PRODUCT_STARTER:
      default:
        version_type_ = SUITE_HOME;
        break;
    }
  } else if (version_info.dwMajorVersion == 5 &&
             version_info.dwMinorVersion == 2) {
    if (version_info.wProductType == VER_NT_WORKSTATION &&
        system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
      version_type_ = SUITE_PROFESSIONAL;
    } else if (version_info.wSuiteMask & VER_SUITE_WH_SERVER) {
      version_type_ = SUITE_HOME;
    } else {
      version_type_ = SUITE_SERVER;
    }
  } else if (version_info.dwMajorVersion == 5 &&
             version_info.dwMinorVersion == 1) {
    if (version_info.wSuiteMask & VER_SUITE_PERSONAL)
      version_type_ = SUITE_HOME;
    else
      version_type_ = SUITE_PROFESSIONAL;
  } else {
    // Windows is pre XP so we don't care but pick a safe default.
    version_type_ = SUITE_HOME;
  }
}

OSInfo::~OSInfo() = default;

Version OSInfo::Kernel32Version() {
  static const Version kernel32_version =
      MajorMinorBuildToVersion(Kernel32BaseVersion().components()[0],
                               Kernel32BaseVersion().components()[1],
                               Kernel32BaseVersion().components()[2]);
  return kernel32_version;
}

OSInfo::VersionNumber OSInfo::Kernel32VersionNumber() {
  DCHECK_EQ(Kernel32BaseVersion().components().size(), 4u);
  static const VersionNumber version = {
      .major = Kernel32BaseVersion().components()[0],
      .minor = Kernel32BaseVersion().components()[1],
      .build = Kernel32BaseVersion().components()[2],
      .patch = Kernel32BaseVersion().components()[3]};
  return version;
}

// Retrieve a version from kernel32. This is useful because when running in
// compatibility mode for a down-level version of the OS, the file version of
// kernel32 will still be the "real" version.
base::Version OSInfo::Kernel32BaseVersion() {
  static const NoDestructor<base::Version> version([] {
    // Allow the calls to `Kernel32BaseVersion()` to block, as they only happen
    // once (after which the result is cached in `version`), and reading from
    // kernel32.dll is fast in practice because it is used by all processes and
    // therefore likely to be in the OS's file cache.
    base::ScopedAllowBlocking allow_blocking;
    std::unique_ptr<FileVersionInfoWin> file_version_info =
        FileVersionInfoWin::CreateFileVersionInfoWin(
            FilePath(FILE_PATH_LITERAL("kernel32.dll")));
    if (!file_version_info) {
      // crbug.com/912061: on some systems it seems kernel32.dll might be
      // corrupted or not in a state to get version info. In this case try
      // kernelbase.dll as a fallback.
      file_version_info = FileVersionInfoWin::CreateFileVersionInfoWin(
          FilePath(FILE_PATH_LITERAL("kernelbase.dll")));
    }
    CHECK(file_version_info);
    return file_version_info->GetFileVersion();
  }());
  return *version;
}

bool OSInfo::IsWowDisabled() const {
  return (wow_process_machine_ == WowProcessMachine::kDisabled);
}

bool OSInfo::IsWowX86OnAMD64() const {
  return (wow_process_machine_ == WowProcessMachine::kX86 &&
          wow_native_machine_ == WowNativeMachine::kAMD64);
}

bool OSInfo::IsWowX86OnARM64() const {
  return (wow_process_machine_ == WowProcessMachine::kX86 &&
          wow_native_machine_ == WowNativeMachine::kARM64);
}

bool OSInfo::IsWowAMD64OnARM64() const {
#if defined(ARCH_CPU_X86_64)
  // An AMD64 process running on an ARM64 device results in the incorrect
  // identification of the device architecture (AMD64 is reported). However,
  // IsWow64Process2 will return the correct device type for the native
  // machine, even though the OS doesn't consider an AMD64 process on an ARM64
  // processor a classic Windows-on-Windows setup.
  return (wow_process_machine_ == WowProcessMachine::kDisabled &&
          wow_native_machine_ == WowNativeMachine::kARM64);
#else
  return false;
#endif
}

bool OSInfo::IsWowX86OnOther() const {
  return (wow_process_machine_ == WowProcessMachine::kX86 &&
          wow_native_machine_ == WowNativeMachine::kOther);
}

std::string OSInfo::processor_model_name() {
  if (processor_model_name_.empty()) {
    const wchar_t kProcessorNameString[] =
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    RegKey key(HKEY_LOCAL_MACHINE, kProcessorNameString, KEY_READ);
    std::wstring value;
    key.ReadValue(L"ProcessorNameString", &value);
    processor_model_name_ = WideToUTF8(value);
  }
  return processor_model_name_;
}

bool OSInfo::IsWindowsNSku() const {
  switch (os_type_) {
    case PRODUCT_BUSINESS_N:
    case PRODUCT_CORE_N:
    case PRODUCT_CORE_CONNECTED_N:
    case PRODUCT_EDUCATION_N:
    case PRODUCT_ENTERPRISE_N:
    case PRODUCT_ENTERPRISE_S_N:
    case PRODUCT_ENTERPRISE_SUBSCRIPTION_N:
    case PRODUCT_HOME_BASIC_N:
    case PRODUCT_HOME_PREMIUM_N:
    case PRODUCT_PRO_FOR_EDUCATION_N:
    case PRODUCT_PRO_WORKSTATION_N:
    case PRODUCT_PROFESSIONAL_N:
    case PRODUCT_PROFESSIONAL_S_N:
    case PRODUCT_PROFESSIONAL_STUDENT_N:
    case PRODUCT_STARTER_N:
    case PRODUCT_ULTIMATE_N:
      return true;
    default:
      return false;
  }
}

// With the exception of Server 2003, server variants are treated the same as
// the corresponding workstation release.
// static
Version OSInfo::MajorMinorBuildToVersion(uint32_t major,
                                         uint32_t minor,
                                         uint32_t build) {
  if (major == 11) {
    // We know nothing about this version of Windows or even if it exists.
    // Known Windows 11 versions have a major number 10 and are thus handled by
    // the == 10 block below.
    return Version::WIN11;
  }

  if (major == 10) {
    if (build >= 26100) {
      return Version::WIN11_24H2;
    }
    if (build >= 22631) {
      return Version::WIN11_23H2;
    }
    if (build >= 22621) {
      return Version::WIN11_22H2;
    }
    if (build >= 22000) {
      return Version::WIN11;
    }
    if (build >= 20348) {
      return Version::SERVER_2022;
    }
    if (build >= 19045) {
      return Version::WIN10_22H2;
    }
    if (build >= 19044) {
      return Version::WIN10_21H2;
    }
    if (build >= 19043) {
      return Version::WIN10_21H1;
    }
    if (build >= 19042) {
      return Version::WIN10_20H2;
    }
    if (build >= 19041) {
      return Version::WIN10_20H1;
    }
    if (build >= 18363) {
      return Version::WIN10_19H2;
    }
    if (build >= 18362) {
      return Version::WIN10_19H1;
    }
    if (build >= 17763) {
      return Version::WIN10_RS5;
    }
    if (build >= 17134) {
      return Version::WIN10_RS4;
    }
    if (build >= 16299) {
      return Version::WIN10_RS3;
    }
    if (build >= 15063) {
      return Version::WIN10_RS2;
    }
    if (build >= 14393) {
      return Version::WIN10_RS1;
    }
    if (build >= 10586) {
      return Version::WIN10_TH2;
    }
    return Version::WIN10;
  }

  if (major > 6) {
    // Hitting this likely means that it's time for a >11 block above.
    LOG(DFATAL) << "Unsupported version: " << major << "." << minor << "."
                << build;

    SCOPED_CRASH_KEY_NUMBER("WindowsVersion", "major", major);
    SCOPED_CRASH_KEY_NUMBER("WindowsVersion", "minor", minor);
    SCOPED_CRASH_KEY_NUMBER("WindowsVersion", "build", build);
    base::debug::DumpWithoutCrashing();

    return Version::WIN_LAST;
  }

  if (major == 6) {
    switch (minor) {
      case 0:
        return Version::VISTA;
      case 1:
        return Version::WIN7;
      case 2:
        return Version::WIN8;
      default:
        DCHECK_EQ(minor, 3u);
        return Version::WIN8_1;
    }
  }

  if (major == 5 && minor != 0) {
    // Treat XP Pro x64, Home Server, and Server 2003 R2 as Server 2003.
    return minor == 1 ? Version::XP : Version::SERVER_2003;
  }

  // Win 2000 or older.
  return Version::PRE_XP;
}

Version GetVersion() {
  return OSInfo::GetInstance()->version();
}

OSInfo::WowProcessMachine OSInfo::GetWowProcessMachineArchitecture(
    const int process_machine) {
  switch (process_machine) {
    case IMAGE_FILE_MACHINE_UNKNOWN:
      return OSInfo::WowProcessMachine::kDisabled;
    case IMAGE_FILE_MACHINE_I386:
      return OSInfo::WowProcessMachine::kX86;
    case IMAGE_FILE_MACHINE_ARM:
    case IMAGE_FILE_MACHINE_THUMB:
    case IMAGE_FILE_MACHINE_ARMNT:
      return OSInfo::WowProcessMachine::kARM32;
  }
  return OSInfo::WowProcessMachine::kOther;
}

OSInfo::WowNativeMachine OSInfo::GetWowNativeMachineArchitecture(
    const int native_machine) {
  switch (native_machine) {
    case IMAGE_FILE_MACHINE_ARM64:
      return OSInfo::WowNativeMachine::kARM64;
    case IMAGE_FILE_MACHINE_AMD64:
      return OSInfo::WowNativeMachine::kAMD64;
  }
  return OSInfo::WowNativeMachine::kOther;
}

void OSInfo::InitializeWowStatusValuesFromLegacyApi(HANDLE process_handle) {
  BOOL is_wow64 = FALSE;
  if (!::IsWow64Process(process_handle, &is_wow64))
    return;
  if (is_wow64) {
    wow_process_machine_ = WowProcessMachine::kX86;
    wow_native_machine_ = WowNativeMachine::kAMD64;
  } else {
    wow_process_machine_ = WowProcessMachine::kDisabled;
  }
}

void OSInfo::InitializeWowStatusValuesForProcess(HANDLE process_handle) {
  static const auto is_wow64_process2 =
      reinterpret_cast<decltype(&IsWow64Process2)>(::GetProcAddress(
          ::GetModuleHandle(L"kernel32.dll"), "IsWow64Process2"));
  if (!is_wow64_process2) {
    InitializeWowStatusValuesFromLegacyApi(process_handle);
    return;
  }

  USHORT process_machine = IMAGE_FILE_MACHINE_UNKNOWN;
  USHORT native_machine = IMAGE_FILE_MACHINE_UNKNOWN;
  if (!is_wow64_process2(process_handle, &process_machine, &native_machine)) {
    return;
  }
  wow_process_machine_ = GetWowProcessMachineArchitecture(process_machine);
  wow_native_machine_ = GetWowNativeMachineArchitecture(native_machine);
}

}  // namespace win
}  // namespace base
