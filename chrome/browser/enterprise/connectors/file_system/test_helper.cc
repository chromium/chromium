// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace enterprise_connectors {

////////////////////////////////////////////////////////////////////////////////
// DownloadItemForTest
////////////////////////////////////////////////////////////////////////////////

const base::FilePath::StringType kCrdownload = FILE_PATH_LITERAL(".crdownload");

DownloadItemForTest::DownloadItemForTest(
    base::FilePath::StringPieceType file_name,
    base::Time::Exploded start_time_exploded) {
  CHECK(file_name.size());
  CHECK(tmp_dir_.CreateUniqueTempDir());
  path_ = tmp_dir_.GetPath().Append(file_name);

  // For GetTargetFilePath():
  CHECK(path_.FinalExtension().size()) << file_name;
  CHECK(path_.FinalExtension() != kCrdownload);
  SetTargetFilePath(path_);

  // For GetFullPath():
  path_ = path_.AddExtension(kCrdownload);
  CHECK(path_.FinalExtension() == kCrdownload);

  // For GetStartTime():
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

DownloadItemForTest::~DownloadItemForTest() = default;

const base::FilePath& DownloadItemForTest::GetFullPath() const {
  return path_;
}

DownloadItemForTest::DownloadState DownloadItemForTest::GetState() const {
  return state_;
}

const DownloadItemRerouteInfo& DownloadItemForTest::GetRerouteInfo() const {
  return rerouted_info_;
}

void DownloadItemForTest::SetRerouteInfo(DownloadItemRerouteInfo info) {
  rerouted_info_ = info;
}

void DownloadItemForTest::SetState(DownloadState state) {
  state_ = state;
}

////////////////////////////////////////////////////////////////////////////////
// MockApiCallFlow
////////////////////////////////////////////////////////////////////////////////
MockApiCallFlow::MockApiCallFlow() = default;
MockApiCallFlow::~MockApiCallFlow() = default;

}  // namespace enterprise_connectors
