// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_chromeos_version_info.h"

#include "base/system/sys_info.h"

namespace base {
namespace test {

ScopedChromeOSVersionInfo::ScopedChromeOSVersionInfo(StringPiece lsb_release,
                                                     Time lsb_release_time) {
  SysInfo::SetChromeOSVersionInfoForTest(lsb_release.as_string(),
                                         lsb_release_time);
}

ScopedChromeOSVersionInfo::~ScopedChromeOSVersionInfo() {
  SysInfo::ResetChromeOSVersionInfoForTest();
}

}  // namespace test
}  // namespace base
