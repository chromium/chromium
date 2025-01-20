// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/drive_info.h"

namespace base {

DriveInfo::DriveInfo() = default;
DriveInfo::DriveInfo(DriveInfo&&) noexcept = default;
DriveInfo& DriveInfo::operator=(DriveInfo&&) noexcept = default;
DriveInfo::~DriveInfo() = default;

}  // namespace base
