// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILE_SYSTEM_WATCHER_FILE_SYSTEM_SCANNER_H_
#define CHROME_BROWSER_ASH_ARC_FILE_SYSTEM_WATCHER_FILE_SYSTEM_SCANNER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace arc {

class ArcBridgeService;

struct RegularScanResult {
  RegularScanResult();
  ~RegularScanResult();
  RegularScanResult(RegularScanResult&&);
  RegularScanResult& operator=(RegularScanResult&&);

  std::vector<std::string> modified_files;
  std::vector<std::string> modified_directories;
};

// Periodical scanner to detect file system modifications in a directory. It is
// activated when FileSystemWatcher throws an error, e.g., the file system is
// too large.
// TODO(risan): Address all remaining feedbacks from next iterations of
// https://chromium-review.googlesource.com/c/chromium/src/+/1946177 before
// enabling this.
class FileSystemScanner {
 public:
  using GetLastChangeTimeCallback =
      base::RepeatingCallback<base::Time(const base::FilePath& path)>;

  // |cros_dir| is the directory that will be scanned by the scanner.
  // |android_dir| is the respective path of |cros_dir| which is mounted to the
  // Android container.
  FileSystemScanner(const base::FilePath& cros_dir,
                    const base::FilePath& android_dir,
                    ArcBridgeService* arc_bridge_service);

  // This constructor is only testing.
  FileSystemScanner(const base::FilePath& cros_dir,
                    const base::FilePath& android_dir,
                    ArcBridgeService* arc_bridge_service,
                    GetLastChangeTimeCallback ctime_callback);

  FileSystemScanner(const FileSystemScanner&) = delete;
  FileSystemScanner& operator=(const FileSystemScanner&) = delete;

  ~FileSystemScanner();

  // Starts scanning the directory.
  void Start();

 private:
  // Schedules a full scan that triggers RequestMediaScan for all files and
  // triggers ReindexDirectory for |android_dir_|. The reason for reindexing is
  // to ensure that there are no indexes for non-existing files in MediaStore.
  void ScheduleFullScan();

  // Called after a full scan is finished. It updates |previous_scan_time_|
  // accordingly. It also  updates the state, so that a regular scan can be
  // scheduled.
  void OnFullScanFinished(base::Time current_scan_time,
                          std::vector<std::string> media_files);

  // Schedules a regular scan if there is no ongoing scan at that time. Regular
  // scan triggers RequestMediaScan only for the files that are modified after
  // the previous (full or regular) scan. It also calls RequestFileRemovalScan
  // for the modified directories to detect the removed files and directories
  // under them. So that, their entries are removed from MediaStore.
  void ScheduleRegularScan();

  // Called after a regular scan is finished. It updates |previous_scan_time_|
  // accordingly. It also updates the state, so that another regular scan can be
  // done without skipping unless there is a full scan scheduled.
  void OnRegularScanFinished(base::Time current_scan_time,
                             RegularScanResult result);

  // Wrapper function that calls ReindexDirectory through mojo interface.
  void ReindexDirectory(const std::string& directory_path);

  // Wrapper function that calls RequestFileRemovalScan through mojo interface.
  void RequestFileRemovalScan(const std::vector<std::string>& directory_paths);

  // Wrapper function that calls RequestMediaScan through mojo interface.
  void RequestMediaScan(const std::vector<std::string>& files);

  // Internal state which is used to skip regular scans when there is an ongoing
  // regular scan or there is a full scan scheduled and has not finished yet.
  enum class State {
    // Neither a regular scan nor full scan is happening. A scan can only be
    // scheduled in this state.
    kIdle,
    // There is a scan which is already PostTask'ed but haven't finished yet.
    // State will return to |kIdle| in the OnScanFinished function.
    kWaitingForScanToFinish,
  };
  State state_;

  const base::FilePath cros_dir_;
  const base::FilePath android_dir_;
  const raw_ptr<ArcBridgeService> arc_bridge_service_;

  // The timestamp of the start of the last scan.
  base::Time previous_scan_time_;
  // The task runner which runs the scans to avoid blocking the UI thread.
  scoped_refptr<base::SequencedTaskRunner> scan_runner_;
  // Calls ScheduleRegularScan every |kFileSystemScannerInterval|.
  base::RepeatingTimer timer_;
  GetLastChangeTimeCallback ctime_callback_;
  base::WeakPtrFactory<FileSystemScanner> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest, ScheduleFullScan);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanCreateTopLevelFile);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanCreateTopLevelDirectory);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanModifyTopLevelFile);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanModifyTopLevelDirectory);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanRenameTopLevelFile);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanRenameTopLevelDirectory);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanDeleteTopLevelFile);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanDeleteTopLevelDirectory);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanCreateNestedFile);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanCreateNestedDirectory);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanModifyNestedFile);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanModifyNestedDirectory);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanRenameNestedFile);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanRenameNestedDirectory);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanDeleteNestedFile);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanDeleteNestedDirectory);
  FRIEND_TEST_ALL_PREFIXES(ArcFileSystemScannerTest,
                           ScheduleRegularScanNoChange);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILE_SYSTEM_WATCHER_FILE_SYSTEM_SCANNER_H_
