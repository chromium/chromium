// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_CHILD_PROCESS_SERVICE_H_
#define BASE_ANDROID_CHILD_PROCESS_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/base_export.h"

namespace base::android {

BASE_EXPORT void RegisterFileDescriptors(
    const std::vector<std::optional<std::string>>& keys,
    const std::vector<int32_t>& ids,
    const std::vector<int32_t>& fds,
    const std::vector<int64_t>& offsets,
    const std::vector<int64_t>& sizes);
BASE_EXPORT void DumpProcessStack();
BASE_EXPORT void OnSelfFreeze();

}  // namespace base::android

#endif  // BASE_ANDROID_CHILD_PROCESS_SERVICE_H_
