// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/windows_version.h"

#include <windows.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

#if !defined(__clang__) && _MSC_FULL_VER < 191125507
#error VS 2017 Update 3.2 or higher is required
#endif

#if !defined(NTDDI_WIN10_VB)
#error Windows 10.0.19041.0 SDK or higher required.
#endif

namespace base {
namespace win {

namespace {

// The values under the CurrentVersion registry hive are mirrored under
// the corresponding Wow6432 hive.
constexpr wchar_t kRegKeyWindowsNTCurrentVersion[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

// Returns the "UBR" (Windows 10 patch number) and "ReleaseId" (Windows 10
// release number) from the registry. "UBR" is an undocumented value and will be
// 0 if the value was not found. "ReleaseId" will be an empty string if the
// value is not found.
std::pair<int, std::string> GetVersionData() {
  DWORD ubr = 0;
  std::wstring release_id;
  RegKey key;

  if (key.Open(HKEY_LOCAL_MACHINE, kRegKeyWindowsNTCurrentVersion,
               KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    key.ReadValueDW(L"UBR", &ubr);
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
  static OSInfo* info = []() {
    _OSVERSIONINFOEXW version_info = {sizeof(version_info)};
    ::GetVersionEx(reinterpret_cast<_OSVERSIONINFOW*>(&version_info));

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

OSInfo::OSInfo(const _OSVERSIONINFOEXW& version_info,
               const _SYSTEM_INFO& system_info,
               int os_type)
    : version_(Version::PRE_XP),
      wow64_status_(GetWOW64StatusForProcess(GetCurrentProcess())) {
  version_number_.major = version_info.dwMajorVersion;
  version_number_.minor = version_info.dwMinorVersion;
  version_number_.build = version_info.dwBuildNumber;
  std::tie(version_number_.patch, release_id_) = GetVersionData();
  version_ = MajorMinorBuildToVersion(
      version_number_.major, version_number_.minor, version_number_.build);
  service_pack_.major = version_info.wServicePackMajor;
  service_pack_.minor = version_info.wServicePackMinor;
  service_pack_str_ = WideToUTF8(version_info.szCSDVersion);

  processors_ = system_info.dwNumberOfProcessors;
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
      case PRODUCT_BUSINESS:
      case PRODUCT_BUSINESS_N:
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

Version OSInfo::Kernel32Version() const {
  static const Version kernel32_version =
      MajorMinorBuildToVersion(Kernel32BaseVersion().components()[0],
                               Kernel32BaseVersion().components()[1],
                               Kernel32BaseVersion().components()[2]);
  return kernel32_version;
}

OSInfo::VersionNumber OSInfo::Kernel32VersionNumber() const {
  DCHECK(Kernel32BaseVersion().components().size() == 4);
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
base::Version OSInfo::Kernel32BaseVersion() const {
  static const NoDestructor<base::Version> version([] {
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

// static
OSInfo::WOW64Status OSInfo::GetWOW64StatusForProcess(HANDLE process_handle) {
  BOOL is_wow64 = FALSE;
  if (!::IsWow64Process(process_handle, &is_wow64))
    return WOW64_UNKNOWN;
  return is_wow64 ? WOW64_ENABLED : WOW64_DISABLED;
}

// With the exception of Server 2003, server variants are treated the same as
// the corresponding workstation release.
// static
Version OSInfo::MajorMinorBuildToVersion(uint32_t major,
                                         uint32_t minor,
                                         uint32_t build) {
  if (major == 10) {
    if (build >= 19043)
      return Version::WIN10_21H1;
    if (build >= 19042)
      return Version::WIN10_20H2;
    if (build >= 19041)
      return Version::WIN10_20H1;
    if (build >= 18363)
      return Version::WIN10_19H2;
    if (build >= 18362)
      return Version::WIN10_19H1;
    if (build >= 17763)
      return Version::WIN10_RS5;
    if (build >= 17134)
      return Version::WIN10_RS4;
    if (build >= 16299)
      return Version::WIN10_RS3;
    if (build >= 15063)
      return Version::WIN10_RS2;
    if (build >= 14393)
      return Version::WIN10_RS1;
    if (build >= 10586)
      return Version::WIN10_TH2;
    return Version::WIN10;
  }

  if (major > 6) {
    // Hitting this likely means that it's time for a >10 block above.
    NOTREACHED() << major << "." << minor << "." << build;
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

}  // namespace win
}  // namespace base
