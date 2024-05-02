// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOG_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/local_data_source.h"
#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_file.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"

namespace ash::cfm {

// Dummy value for when a call to stat() fails to obtain
// a valid inode. Unlikely to be used, but be defensive.
inline constexpr int kInvalidFileInode = -1;

// This class tracks data from a single log file.
class LogSource : public LocalDataSource {
 public:
  LogSource(const std::string& filepath,
            base::TimeDelta poll_rate,
            size_t batch_size);
  LogSource(const LogSource&) = delete;
  LogSource& operator=(const LogSource&) = delete;
  ~LogSource() override;

  // LocalDataSource:
  const std::string& GetDisplayName() override;
  std::vector<std::string> GetNextData() override;

 protected:
  int GetCurrentFileInode();
  bool DidFileRotate();

 private:
  std::string filepath_;

  // Contains a handle to the log file on disk
  // TODO(b/320996557): this should be a collection of log files
  // after adding rotation support.
  LogFile log_file_;

  // Number of lines to read from the log file at each iteration.
  const size_t batch_size_;

  // Keep track of the last-known inode to detect when the underlying
  // file has rotated. Inodes will not change when the file is renamed.
  int last_known_inode_ = kInvalidFileInode;

  // Must be the last class member.
  base::WeakPtrFactory<LogSource> weak_ptr_factory_{this};
};

}  // namespace ash::cfm
#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOG_SOURCE_H_
