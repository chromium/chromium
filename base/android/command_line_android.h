// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_COMMAND_LINE_ANDROID_H_
#define BASE_ANDROID_COMMAND_LINE_ANDROID_H_

#include <string>
#include <vector>

#include "base/base_export.h"

namespace base::android {
BASE_EXPORT void CommandLineInit(std::vector<std::string>& command_line);
}  // namespace base::android

#endif  // BASE_ANDROID_COMMAND_LINE_ANDROID_H_
