// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_file.h"

namespace ash {

HoldingSpaceFile::HoldingSpaceFile(const base::FilePath& file_path,
                                   FileSystemType file_system_type,
                                   const GURL& file_system_url)
    : file_path(file_path),
      file_system_type(file_system_type),
      file_system_url(file_system_url) {}

HoldingSpaceFile::HoldingSpaceFile(const HoldingSpaceFile&) = default;

HoldingSpaceFile::HoldingSpaceFile(HoldingSpaceFile&&) = default;

HoldingSpaceFile& HoldingSpaceFile::operator=(const HoldingSpaceFile&) =
    default;

HoldingSpaceFile& HoldingSpaceFile::operator=(HoldingSpaceFile&&) = default;

HoldingSpaceFile::~HoldingSpaceFile() = default;

bool HoldingSpaceFile::operator==(const HoldingSpaceFile& rhs) const {
  return std::tie(file_path, file_system_type, file_system_url) ==
         std::tie(rhs.file_path, rhs.file_system_type, rhs.file_system_url);
}

bool HoldingSpaceFile::operator!=(const HoldingSpaceFile& rhs) const {
  return !(*this == rhs);
}

}  // namespace ash
