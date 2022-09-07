// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_running_on_chromeos.h"

#include "base/system/sys_info.h"
#include "base/time/time.h"

namespace base {
namespace test {
namespace {

// Chrome OS /etc/lsb-release values that make SysInfo::IsRunningOnChromeOS()
// return true.
const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

}  // namespace

ScopedRunningOnChromeOS::ScopedRunningOnChromeOS() {
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, Time());
}

ScopedRunningOnChromeOS::~ScopedRunningOnChromeOS() {
  SysInfo::ResetChromeOSVersionInfoForTest();
}

}  // namespace test
}  // namespace base
