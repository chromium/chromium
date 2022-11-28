// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/file_system_watcher/file_system_scanner.h"

#include <fts.h>
#include <sys/stat.h>

#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/files/file_enumerator.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/file_system_watcher/arc_file_system_watcher_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace arc {

namespace {

struct FTSCloser {
  void operator()(FTS* fts) {
    if (fts)
      fts_close(fts);
  }
};

// The scan interval for FileSystemScanner instances.
// This value needs to be bigger than 1 second since we subtract 1 second
// inside IsModified function to handle the timekeeper issue. In order to get
// more information about the issue, please check the comments above IsModified
// function.
//
// TODO(crbug/1037824): Measure the battery usage to find an optimal value for
// this.
constexpr base::TimeDelta kRegularScanInterval = base::Seconds(5);

// This value is used to handle the delay caused by timekeeper when reading the
// ctime values. For more information, please read the comments inside
// IsModified function.
//
// This value MUST NOT exceed |kRegularScanInterval|.
constexpr base::TimeDelta kCtimeCorrection = base::Seconds(1);

// Returns ctime for the file |path| using stat(2).
// If stat fails for some reason, e.g., the file does not exists, then it
// returns base::Time().
base::Time GetLastChangedTime(const base::FilePath& path) {
  struct stat st = {};
  const int res = stat(path.value().c_str(), &st);
  if (res < 0) {
    DPLOG(ERROR) << "Couldn't stat " << path.value();
    return base::Time();
  }
  return base::Time::FromTimeSpec(st.st_ctim);
}

// Returns true if ctime of |path| is after |previous_scan_time|.
bool IsModified(const base::FilePath& path,
                base::Time previous_scan_time,
                FileSystemScanner::GetLastChangeTimeCallback ctime_callback) {
  // It is possible that ctime of the file might be before |previous_scan_time|
  // even if the file has been modified after |previous_scan_time|.
  //
  // The reason for this is the concept of timekeeping in linux kernel. When
  // writing to a file, ctime is updated using the value from the timekeeper
  // which is updated by interrupts. However, when we use base::Time::Now() to
  // update |previous_scan_time|, that call reads the system clock time directly
  // instead of reading it from the timekeeper. Therefore, if the file is
  // modified just after |previous_scan_time| is updated, it is possible
  // that ctime is updated before an interrupt has been triggered.
  //
  // Thus, we add |kCtimeCorrection| to the ctime before the comparison to
  // ensure we do not miss any modifications.
  //
  // Since |previous_scan_time| is updated at the beginning of a scan, it might
  // be possible for a file to be identified as modified twice if it is modified
  // after the |previous_scan_time| is stored but before the file is checked by
  // the scan. It is expected and preferred compared to miss a file
  // modification, e.g., if we store |previous_scan_time| at the end of a scan.
  base::Time ctime = ctime_callback.Run(path) + kCtimeCorrection;
  return previous_scan_time <= ctime;
}

// The following functions are made non-member functions because they can be
// executed on any thread other than the UI thread, and the FileSystemScanner is
// created on UI thread.
//
// It is not possible to PostTask an object's member function to a thread
// other than the one where the object is allocated, using weak pointers.

// Returns Android paths of all media files (recursively) under |directory|.
std::vector<std::string> FullScan(const base::FilePath& directory,
                                  base::Time previous_scan_time,
                                  const base::FilePath& cros_dir,
                                  const base::FilePath& android_dir) {
  std::vector<std::string> media_files;
  base::FileEnumerator file_enum(directory, true /* recursive */,
                                 base::FileEnumerator::FILES);
  for (auto path = file_enum.Next(); !path.empty(); path = file_enum.Next()) {
    if (!HasAndroidSupportedMediaExtension(path))
      continue;
    media_files.push_back(GetAndroidPath(path, cros_dir, android_dir).value());
  }
  return media_files;
}

// Iterates over the files in |cros_dir| and calls respective mojo functions
// for modified files and directories.
RegularScanResult RegularScan(
    base::Time previous_scan_time,
    const base::FilePath& cros_dir,
    const base::FilePath& android_dir,
    FileSystemScanner::GetLastChangeTimeCallback ctime_callback) {
  char* argv[] = {const_cast<char*>(cros_dir.value().c_str()), nullptr};
  std::unique_ptr<FTS, FTSCloser> fts{
      fts_open(argv, FTS_PHYSICAL | FTS_NOCHDIR | FTS_XDEV, nullptr)};
  if (!fts) {
    LOG(ERROR) << "Regular scan failed because the path cannot be opened.";
    return {};
  }

  RegularScanResult result;
  // Keeps track of the last modified directory to force RequestMediaScan for
  // all media files under a modified directory.
  base::FilePath topmost_modified_dir;

  // FileEnumerator does not guarantee the traversal ordering. Thus, fts_read is
  // preferred here since it provides the preorder traversal which is important
  // to cache |topmost_modified_dir|.
  FTSENT* p;
  while ((p = fts_read(fts.get())) != nullptr) {
    // There is no fts* MSAN interceptor yet in the codebase, so this
    // manual MSAN_UNPOISON needs to be done to avoid false MSAN test runs
    // failures.
    MSAN_UNPOISON(p, sizeof(FTSENT));
    MSAN_UNPOISON(p->fts_path, p->fts_pathlen + 1);
    base::FilePath path(p->fts_path);
    if (p->fts_info == FTS_D) {
      if (!IsModified(path, previous_scan_time, ctime_callback))
        continue;
      if (!topmost_modified_dir.IsParent(path))
        topmost_modified_dir = path;
      // Since the directory is modified, push it into the buffer to search for
      // removed files and remove their entries from MediaStore.
      result.modified_directories.push_back(
          GetAndroidPath(path, cros_dir, android_dir).value());
    } else if (p->fts_info == FTS_F &&
               HasAndroidSupportedMediaExtension(path)) {
      if (IsModified(path, previous_scan_time, ctime_callback) ||
          topmost_modified_dir.IsParent(path)) {
        // If it is modified or its under a directory which is modified, push it
        // into the buffer to trigger media scan later.
        result.modified_files.push_back(
            GetAndroidPath(path, cros_dir, android_dir).value());
      }
    }
  }
  return result;
}

}  // namespace

RegularScanResult::RegularScanResult() = default;
RegularScanResult::~RegularScanResult() = default;
RegularScanResult::RegularScanResult(RegularScanResult&&) = default;
RegularScanResult& RegularScanResult::operator=(RegularScanResult&&) = default;

FileSystemScanner::FileSystemScanner(const base::FilePath& cros_dir,
                                     const base::FilePath& android_dir,
                                     ArcBridgeService* arc_bridge_service)
    : FileSystemScanner(cros_dir,
                        android_dir,
                        arc_bridge_service,
                        base::BindRepeating(&GetLastChangedTime)) {}

FileSystemScanner::FileSystemScanner(const base::FilePath& cros_dir,
                                     const base::FilePath& android_dir,
                                     ArcBridgeService* arc_bridge_service,
                                     GetLastChangeTimeCallback ctime_callback)
    : state_(State::kIdle),
      cros_dir_(cros_dir),
      android_dir_(android_dir),
      arc_bridge_service_(arc_bridge_service),
      scan_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      ctime_callback_(ctime_callback) {}

FileSystemScanner::~FileSystemScanner() = default;

void FileSystemScanner::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ScheduleFullScan();
}

// TODO(risan): Consider to rename this to "Post".
void FileSystemScanner::ScheduleFullScan() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Stop the timer to prevent scheduling regular scans before the full scan
  // finishes.
  timer_.Stop();
  // Invalidate WeakPtrs to cancel callbacks to prevent the scanner being idle.
  weak_ptr_factory_.InvalidateWeakPtrs();
  state_ = State::kWaitingForScanToFinish;
  // TODO(risan): We could skip a bunch of FullScan queued in the sequence if
  // several SystemTimeClock change happened later (in separate CL). To do the
  // skip, we could have a boolean variable full_scan_requested, and PostTask
  // full_scan_requested from the OnFullScanFinished.
  scan_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FullScan, cros_dir_, previous_scan_time_, cros_dir_,
                     android_dir_),
      base::BindOnce(&FileSystemScanner::OnFullScanFinished,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now()));
  ReindexDirectory(android_dir_.value());
}

void FileSystemScanner::OnFullScanFinished(
    base::Time current_scan_time,
    std::vector<std::string> media_files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state_, State::kWaitingForScanToFinish);
  if (!media_files.empty())
    RequestMediaScan(media_files);
  previous_scan_time_ = current_scan_time;
  state_ = State::kIdle;
  timer_.Start(FROM_HERE, kRegularScanInterval, this,
               &FileSystemScanner::ScheduleRegularScan);
}

void FileSystemScanner::ScheduleRegularScan() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do not schedule another scan if there is an ongoing scan happening.
  if (state_ != State::kIdle)
    return;
  state_ = State::kWaitingForScanToFinish;
  scan_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RegularScan, previous_scan_time_, cros_dir_, android_dir_,
                     ctime_callback_),
      base::BindOnce(&FileSystemScanner::OnRegularScanFinished,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now()));
}

void FileSystemScanner::OnRegularScanFinished(base::Time current_scan_time,
                                              RegularScanResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(state_, State::kIdle);
  if (!result.modified_files.empty())
    RequestMediaScan(result.modified_files);
  if (!result.modified_directories.empty())
    RequestFileRemovalScan(result.modified_directories);
  previous_scan_time_ = current_scan_time;
  state_ = State::kIdle;
}

void FileSystemScanner::ReindexDirectory(const std::string& directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), ReindexDirectory);
  if (!instance) {
    LOG(WARNING) << "Failed to call ReindexDirectory.";
    return;
  }
  instance->ReindexDirectory(directory);
}

void FileSystemScanner::RequestFileRemovalScan(
    const std::vector<std::string>& directories) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), RequestFileRemovalScan);
  if (!instance) {
    LOG(WARNING) << "Failed to call RequestFileRemovalScan.";
    return;
  }
  instance->RequestFileRemovalScan(directories);
}

void FileSystemScanner::RequestMediaScan(
    const std::vector<std::string>& files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), RequestMediaScan);
  if (!instance) {
    LOG(WARNING) << "Failed to call RequestMediaScan.";
    return;
  }
  instance->RequestMediaScan(files);
}

}  // namespace arc
