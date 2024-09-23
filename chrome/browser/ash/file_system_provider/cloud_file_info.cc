// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/cloud_file_info.h"

namespace ash::file_system_provider {

CloudFileInfo::CloudFileInfo(const std::string& version_tag)
    : version_tag(version_tag) {}

CloudFileInfo::~CloudFileInfo() = default;

bool CloudFileInfo::operator==(const CloudFileInfo& other) const {
  return version_tag == other.version_tag;
}

}  // namespace ash::file_system_provider
