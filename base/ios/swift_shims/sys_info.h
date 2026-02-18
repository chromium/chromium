// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IOS_SWIFT_SHIMS_SYS_INFO_H_
#define BASE_IOS_SWIFT_SHIMS_SYS_INFO_H_

#include <string>

namespace base {
namespace swift {

std::string GetIOSBuildNumber();

}  // namespace swift
}  // namespace base

#endif  // BASE_IOS_SWIFT_SHIMS_SYS_INFO_H_
