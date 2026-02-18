// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/swift_shims/sys_info.h"

#include "base/system/sys_info.h"

namespace base {
namespace swift {

std::string GetIOSBuildNumber() {
  return base::SysInfo::GetIOSBuildNumber();
}

}  // namespace swift
}  // namespace base
