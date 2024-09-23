// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_LOG_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/local_data_source.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/log_file.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/persistent_db.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"

namespace ash::cfm {

inline constexpr char kCfmChromeLogFile[] = "/var/log/chrome/chrome";
inline constexpr char kCfmCrosEcLogFile[] = "/var/log/cros_ec.log";
inline constexpr char kCfmFwupdLogFile[] = "/var/log/fwupd.log";
inline constexpr char kCfmPowerdLogFile[] = "/var/log/powerd.out";
inline constexpr char kCfmSyslogLogFile[] = "/var/log/messages";
inline constexpr char kCfmUiLogFile[] = "/var/log/ui/ui.LATEST";
inline constexpr char kCfmUpdateEngineLogFile[] = "/var/log/update_engine.log";

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
  void Fetch(FetchCallback callback) override;
  void Flush() override;
  const std::string& GetDisplayName() override;
  std::vector<std::string> GetNextData() override;

  // Getter that returns the proper LogSource child class depending
  // on the provided filename.
  static std::unique_ptr<LogSource> Create(const std::string& filename,
                                           base::TimeDelta poll_rate,
                                           size_t batch_size);

 protected:
  int GetCurrentFileInode();
  bool DidFileRotate();

 private:
  std::streampos GetLastKnownOffsetFromStorage();
  void PersistCurrentOffsetToStorage();

  std::string filepath_;

  // Set to true if we can access the file and false if not.
  // If false, all future operations are no-ops.
  bool file_is_accessible_ = true;

  // Contains a handle to the log file on disk
  // TODO(b/320996557): this should be a collection of log files
  // after adding rotation support.
  LogFile log_file_;

  // Number of lines to read from the log file at each iteration.
  const size_t batch_size_;

  // Keep track of the last-known inode to detect when the underlying
  // file has rotated. Inodes will not change when the file is renamed.
  int last_known_inode_ = kInvalidFileInode;

  // File offset to seek to after a crash or reboot.
  // Cached here in memory, then flushed to disk on successful log upload.
  std::streampos recovery_offset_ = 0;

  // Must be the last class member.
  base::WeakPtrFactory<LogSource> weak_ptr_factory_{this};
};

}  // namespace ash::cfm
#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_LOG_SOURCE_H_
