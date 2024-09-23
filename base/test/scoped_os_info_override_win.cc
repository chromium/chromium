// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_os_info_override_win.h"

#include <windows.h>

#include "base/win/windows_version.h"

namespace base {
namespace test {

ScopedOSInfoOverride::ScopedOSInfoOverride(Type type)
    : original_info_(base::win::OSInfo::GetInstance()),
      overriding_info_(CreateInfoOfType(type)) {
  *base::win::OSInfo::GetInstanceStorage() = overriding_info_.get();
}

ScopedOSInfoOverride::~ScopedOSInfoOverride() {
  *base::win::OSInfo::GetInstanceStorage() = original_info_;
}

// static
ScopedOSInfoOverride::UniqueOsInfo ScopedOSInfoOverride::CreateInfoOfType(
    Type type) {
  _OSVERSIONINFOEXW version_info = {sizeof(version_info)};
  _SYSTEM_INFO system_info = {};
  int os_type = 0;

  switch (type) {
    case Type::kWin11Pro:
    case Type::kWin11Home:
    case Type::kWin11HomeN:
      version_info.dwMajorVersion = 10;
      version_info.dwMinorVersion = 0;
      version_info.dwBuildNumber = 22000;
      version_info.wServicePackMajor = 0;
      version_info.wServicePackMinor = 0;
      version_info.szCSDVersion[0] = 0;
      version_info.wProductType = VER_NT_WORKSTATION;
      version_info.wSuiteMask = VER_SUITE_PERSONAL;

      system_info.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
      system_info.dwNumberOfProcessors = 1;
      system_info.dwAllocationGranularity = 8;

      os_type = type == Type::kWin11Home    ? PRODUCT_HOME_BASIC
                : type == Type::kWin11HomeN ? PRODUCT_HOME_BASIC_N
                                            : PRODUCT_PROFESSIONAL;
      break;
    case Type::kWinServer2022:
      version_info.dwMajorVersion = 10;
      version_info.dwMinorVersion = 0;
      version_info.dwBuildNumber = 20348;
      version_info.wServicePackMajor = 0;
      version_info.wServicePackMinor = 0;
      version_info.szCSDVersion[0] = 0;
      version_info.wProductType = VER_NT_SERVER;
      version_info.wSuiteMask = VER_SUITE_ENTERPRISE;

      system_info.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
      system_info.dwNumberOfProcessors = 4;
      system_info.dwAllocationGranularity = 64 * 1024;

      os_type = PRODUCT_STANDARD_SERVER;
      break;
    case Type::kWin10Pro21H1:
    case Type::kWin10Pro:
    case Type::kWin10Home:
      version_info.dwMajorVersion = 10;
      version_info.dwMinorVersion = 0;
      version_info.dwBuildNumber = type == Type::kWin10Pro21H1 ? 19043 : 15063;
      version_info.wServicePackMajor = 0;
      version_info.wServicePackMinor = 0;
      version_info.szCSDVersion[0] = 0;
      version_info.wProductType = VER_NT_WORKSTATION;
      version_info.wSuiteMask = VER_SUITE_PERSONAL;

      system_info.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
      system_info.dwNumberOfProcessors = 1;
      system_info.dwAllocationGranularity = 8;

      os_type =
          type == Type::kWin10Home ? PRODUCT_HOME_BASIC : PRODUCT_PROFESSIONAL;
      break;
    case Type::kWinServer2016:
      version_info.dwMajorVersion = 10;
      version_info.dwMinorVersion = 0;
      version_info.dwBuildNumber = 17134;
      version_info.wServicePackMajor = 0;
      version_info.wServicePackMinor = 0;
      version_info.szCSDVersion[0] = 0;
      version_info.wProductType = VER_NT_SERVER;
      version_info.wSuiteMask = VER_SUITE_ENTERPRISE;

      system_info.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
      system_info.dwNumberOfProcessors = 4;
      system_info.dwAllocationGranularity = 64 * 1024;

      os_type = PRODUCT_STANDARD_SERVER;
      break;
  }

  return UniqueOsInfo(new base::win::OSInfo(version_info, system_info, os_type),
                      &ScopedOSInfoOverride::deleter);
}

// static
void ScopedOSInfoOverride::deleter(base::win::OSInfo* info) {
  delete info;
}

}  // namespace test
}  // namespace base
