// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"

#include "base/files/file_path.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "ui/gfx/image/image_skia.h"

namespace policy {

DlpConfidentialFile::DlpConfidentialFile(const base::FilePath& file_path)
    :  // TODO(crbug.com/1357593): Pass the proper |dark_background|
      icon(chromeos::GetIconForPath(file_path, /*dark_background=*/false)),
      title(file_path.BaseName().LossyDisplayName()),
      file_path(file_path) {}

DlpConfidentialFile::DlpConfidentialFile(const DlpConfidentialFile& other) =
    default;
DlpConfidentialFile& DlpConfidentialFile::operator=(
    const DlpConfidentialFile& other) = default;

bool DlpConfidentialFile::operator==(const DlpConfidentialFile& other) const {
  return file_path == other.file_path;
}

bool DlpConfidentialFile::operator!=(const DlpConfidentialFile& other) const {
  return !(*this == other);
}

}  // namespace policy
