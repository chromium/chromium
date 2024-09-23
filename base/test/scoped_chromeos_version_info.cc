// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_chromeos_version_info.h"

#include <string_view>

#include "base/system/sys_info.h"

namespace base {
namespace test {

ScopedChromeOSVersionInfo::ScopedChromeOSVersionInfo(
    std::string_view lsb_release,
    Time lsb_release_time) {
  SysInfo::SetChromeOSVersionInfoForTest(std::string(lsb_release),
                                         lsb_release_time);
}

ScopedChromeOSVersionInfo::~ScopedChromeOSVersionInfo() {
  SysInfo::ResetChromeOSVersionInfoForTest();
}

}  // namespace test
}  // namespace base
