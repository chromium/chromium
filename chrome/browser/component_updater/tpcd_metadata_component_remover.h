// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TPCD_METADATA_COMPONENT_REMOVER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TPCD_METADATA_COMPONENT_REMOVER_H_

#include "base/files/file_path.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

inline constexpr base::FilePath::CharType kTpcdMetadataComponentFileName[] =
    FILE_PATH_LITERAL("metadata.pb");

// TODO(crbug.com/477611289): Remove this code in M147+.
void DeleteTPCDMetadataComponent(const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TPCD_METADATA_COMPONENT_REMOVER_H_
