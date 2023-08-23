// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONFIDENTIAL_FILE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONFIDENTIAL_FILE_H_

#include <string>

#include "base/files/file_path.h"
#include "ui/gfx/image/image_skia.h"

namespace policy {

// Keeps track of title and corresponding icon of a file.
// Used to show information about confidential file to the user.
struct DlpConfidentialFile {
  DlpConfidentialFile() = delete;
  explicit DlpConfidentialFile(const base::FilePath& file_path);
  DlpConfidentialFile(const DlpConfidentialFile& other);
  DlpConfidentialFile& operator=(const DlpConfidentialFile& other);
  ~DlpConfidentialFile() = default;

  // Files with the same file_path are considered equal.
  bool operator==(const DlpConfidentialFile& other) const;
  bool operator!=(const DlpConfidentialFile& other) const;
  bool operator<(const DlpConfidentialFile& other) const;
  bool operator<=(const DlpConfidentialFile& other) const;
  bool operator>(const DlpConfidentialFile& other) const;
  bool operator>=(const DlpConfidentialFile& other) const;

  // File icon used to display in the warning/error dialog.
  gfx::ImageSkia icon;
  // File name used to display in the warning/error dialog.
  std::u16string title;
  // File path used to retrieve |icon| and |title|.
  base::FilePath file_path;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONFIDENTIAL_FILE_H_
