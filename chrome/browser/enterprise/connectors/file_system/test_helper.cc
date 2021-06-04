// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace enterprise_connectors {

////////////////////////////////////////////////////////////////////////////////
// DownloadItemForTest
////////////////////////////////////////////////////////////////////////////////

DownloadItemForTest::DownloadItemForTest(
    base::FilePath::StringPieceType file_name,
    base::Time::Exploded start_time_exploded) {
  CHECK(file_name.size());
  CHECK(tmp_dir_.CreateUniqueTempDir());
  file_path_ = tmp_dir_.GetPath().Append(file_name);
  CHECK(file_path_.FinalExtension().size()) << file_name;
  SetTargetFilePath(file_path_);
  file_path_ = file_path_.AddExtension(FILE_PATH_LITERAL(".crdownload"));
  CHECK(file_path_.FinalExtension() == FILE_PATH_LITERAL(".crdownload"));
  base::Time start_time;
  DCHECK(base::Time::FromLocalExploded(start_time_exploded, &start_time));
  SetStartTime(start_time);
  DLOG(INFO) << "Set start_time for DownloadItemForTest to " << start_time
             << "(" << start_time.ToDeltaSinceWindowsEpoch().InMicroseconds()
             << ")"
             << " from "
             << base::StringPrintf(
                    " - %04d-%02d-%02dT%02d%02d%02d.%03d",
                    start_time_exploded.year, start_time_exploded.month,
                    start_time_exploded.day_of_month, start_time_exploded.hour,
                    start_time_exploded.minute, start_time_exploded.second,
                    start_time_exploded.millisecond);
}

const base::FilePath& DownloadItemForTest::GetFullPath() const {
  return file_path_;
}

}  // namespace enterprise_connectors
