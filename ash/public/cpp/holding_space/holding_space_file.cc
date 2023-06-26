// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_file.h"

namespace ash {

HoldingSpaceFile::HoldingSpaceFile(FileSystemType file_system_type)
    : file_system_type(file_system_type) {}

HoldingSpaceFile::HoldingSpaceFile(const HoldingSpaceFile&) = default;

HoldingSpaceFile::HoldingSpaceFile(HoldingSpaceFile&&) = default;

HoldingSpaceFile& HoldingSpaceFile::operator=(const HoldingSpaceFile&) =
    default;

HoldingSpaceFile& HoldingSpaceFile::operator=(HoldingSpaceFile&&) = default;

HoldingSpaceFile::~HoldingSpaceFile() = default;

bool HoldingSpaceFile::operator==(const HoldingSpaceFile& rhs) const {
  return file_system_type == rhs.file_system_type;
}

bool HoldingSpaceFile::operator!=(const HoldingSpaceFile& rhs) const {
  return !(*this == rhs);
}

}  // namespace ash
