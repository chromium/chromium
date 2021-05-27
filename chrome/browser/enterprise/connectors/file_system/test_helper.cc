// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"

namespace enterprise_connectors {

////////////////////////////////////////////////////////////////////////////////
// DownloadItemForTest
////////////////////////////////////////////////////////////////////////////////

DownloadItemForTest::DownloadItemForTest(
    base::FilePath::StringPieceType file_name) {
  CHECK(file_name.size());
  CHECK(tmp_dir_.CreateUniqueTempDir());
  file_path_ = tmp_dir_.GetPath().Append(file_name);
  CHECK(file_path_.FinalExtension().size()) << file_name;
  SetTargetFilePath(file_path_);
  file_path_ = file_path_.AddExtension(FILE_PATH_LITERAL(".crdownload"));
  CHECK(file_path_.FinalExtension() == FILE_PATH_LITERAL(".crdownload"));
}

const base::FilePath& DownloadItemForTest::GetFullPath() const {
  return file_path_;
}

}  // namespace enterprise_connectors
