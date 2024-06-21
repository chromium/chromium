// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/file_system_watcher/arc_file_system_watcher_service.h"

#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/file_system_watcher/arc_file_system_watcher_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// Mapping from Android file paths to last modified timestamps.
using TimestampMap = std::map<base::FilePath, base::Time>;

namespace arc {

namespace {

// The storage path inside ARC container. This will be the path that is used in
// MediaScanner.scanFile request.
constexpr base::FilePath::CharType kAndroidStorageDir[] =
    FILE_PATH_LITERAL("/storage");

// The Downloads path inside ARC container. This will be the path that
// is used in MediaScanner.scanFile request.
constexpr base::FilePath::CharType kAndroidDownloadDir[] =
    FILE_PATH_LITERAL("/storage/emulated/0/Download/");

// TODO(crbug.com/929031): Move this to arc_volume_mounter_bridge.h.
// The MyFiles path inside ARC container. This will be the path that is used in
// MediaScanner.scanFile request. UUID for the MyFiles volume is taken from
// ash/components/arc/volume_mounter/arc_volume_mounter_bridge.cc.
constexpr base::FilePath::CharType kAndroidMyFilesDir[] =
    FILE_PATH_LITERAL("/storage/0000000000000000000000000000CAFEF00D2019");

// The path for Downloads under MyFiles inside ARC container.
constexpr base::FilePath::CharType kAndroidMyFilesDownloadsDir[] =
    FILE_PATH_LITERAL(
        "/storage/0000000000000000000000000000CAFEF00D2019/Downloads/");

// How long to wait for new inotify events before building the updated timestamp
// map.
const base::TimeDelta kBuildTimestampMapDelay = base::Milliseconds(1000);

// Providing the similar guarantee as
// /proc/sys/fs/inotify/max_queued_events
// It probably does not make sense to store more than the max queued limit in
// inotify, since the inotify system degrades when that happens anyway.
const size_t kMaxTimestampMapSize = 16384;

// Compares two TimestampMaps and returns the list of file paths added/removed
// or whose timestamp have changed.
std::vector<base::FilePath> CollectChangedPaths(
    const TimestampMap& timestamp_map_a,
    const TimestampMap& timestamp_map_b) {
  std::vector<base::FilePath> changed_paths;

  TimestampMap::const_iterator iter_a = timestamp_map_a.begin();
  TimestampMap::const_iterator iter_b = timestamp_map_b.begin();
  while (iter_a != timestamp_map_a.end() && iter_b != timestamp_map_b.end()) {
    if (iter_a->first == iter_b->first) {
      if (iter_a->second != iter_b->second) {
        changed_paths.emplace_back(iter_a->first);
      }
      ++iter_a;
      ++iter_b;
    } else if (iter_a->first < iter_b->first) {
      changed_paths.emplace_back(iter_a->first);
      ++iter_a;
    } else {  // iter_a->first > iter_b->first
      changed_paths.emplace_back(iter_b->first);
      ++iter_b;
    }
  }

  while (iter_a != timestamp_map_a.end()) {
    changed_paths.emplace_back(iter_a->first);
    ++iter_a;
  }
  while (iter_b != timestamp_map_b.end()) {
    changed_paths.emplace_back(iter_b->first);
    ++iter_b;
  }

  return changed_paths;
}

// Scans files under |cros_dir| recursively and builds a map from
// file paths (in Android filesystem) to last modified timestamps.
TimestampMap BuildTimestampMap(base::FilePath cros_dir,
                               base::FilePath android_dir) {
  DCHECK(!cros_dir.EndsWithSeparator());
  TimestampMap timestamp_map;

  // Enumerate normal files only; directories and symlinks are skipped.
  base::FileEnumerator enumerator(cros_dir, true,
                                  base::FileEnumerator::FILES);
  for (base::FilePath cros_path = enumerator.Next(); !cros_path.empty();
       cros_path = enumerator.Next()) {
    if (timestamp_map.size() >= kMaxTimestampMapSize) {
      LOG(WARNING) << "The timestamp map size exceeds max limit";
      break;
    }
    // Skip non-media files for efficiency.
    if (!HasAndroidSupportedMediaExtension(cros_path))
      continue;

    base::FilePath android_path(android_dir);
    cros_dir.AppendRelativePath(cros_path, &android_path);

    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    timestamp_map[android_path] = info.GetLastModifiedTime();
  }
  return timestamp_map;
}

std::pair<base::TimeTicks, TimestampMap> BuildTimestampMapCallback(
    base::FilePath cros_dir,
    base::FilePath android_dir) {
  // The TimestampMap may include changes form after snapshot_time.
  // We must take the snapshot_time before we build the TimestampMap since
  // changes that occur while building the map may not be captured.
  base::TimeTicks snapshot_time = base::TimeTicks::Now();
  TimestampMap current_timestamp_map =
      BuildTimestampMap(cros_dir, android_dir);
  return std::make_pair(snapshot_time, std::move(current_timestamp_map));
}

// Singleton factory for ArcFileSystemWatcherService.
class ArcFileSystemWatcherServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcFileSystemWatcherService,
          ArcFileSystemWatcherServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcFileSystemWatcherServiceFactory";

  ArcFileSystemWatcherServiceFactory()
      : ArcBrowserContextKeyedServiceFactoryBase<
            ArcFileSystemWatcherService,
            ArcFileSystemWatcherServiceFactory>() {
    DependsOn(ArcVolumeMounterBridge::GetFactory());
  }

  static ArcFileSystemWatcherServiceFactory* GetInstance() {
    return base::Singleton<ArcFileSystemWatcherServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcFileSystemWatcherServiceFactory>;
  ~ArcFileSystemWatcherServiceFactory() override = default;
};

}  // namespace

// The core part of ArcFileSystemWatcherService to watch for file changes in
// directory.
class ArcFileSystemWatcherService::FileSystemWatcher {
 public:
  using Callback =
      base::RepeatingCallback<void(const std::vector<std::string>& paths)>;

  FileSystemWatcher(const Callback& callback,
                    const base::FilePath& cros_dir,
                    const base::FilePath& android_dir);

  FileSystemWatcher(const FileSystemWatcher&) = delete;
  FileSystemWatcher& operator=(const FileSystemWatcher&) = delete;

  ~FileSystemWatcher();

  // Starts watching directory.
  void Start();

 private:
  // Called by base::FilePathWatcher to notify file changes.
  // Kicks off the update of last_timestamp_map_ if one is not already in
  // progress.
  void OnFilePathChanged(const base::FilePath& path, bool error);

  // Called with a delay to allow additional inotify events for the same user
  // action to queue up so that they can be dealt with in batch.
  void DelayBuildTimestampMap();

  // Called after a new timestamp map has been created and causes any recently
  // modified files to be sent to the media scanner.
  void OnBuildTimestampMap(
      std::pair<base::TimeTicks, TimestampMap> timestamp_and_map);

  Callback callback_;
  const base::FilePath cros_dir_;
  const base::FilePath android_dir_;
  std::unique_ptr<base::FilePathWatcher> watcher_;
  TimestampMap last_timestamp_map_;
  // The timestamp of the last OnFilePathChanged callback received.
  base::TimeTicks last_notify_time_;
  // Whether or not there is an outstanding task to update last_timestamp_map_.
  bool outstanding_task_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileSystemWatcher> weak_ptr_factory_{this};
};

ArcFileSystemWatcherService::FileSystemWatcher::FileSystemWatcher(
    const Callback& callback,
    const base::FilePath& cros_dir,
    const base::FilePath& android_dir)
    : callback_(callback),
      cros_dir_(cros_dir),
      android_dir_(android_dir),
      last_notify_time_(base::TimeTicks()),
      outstanding_task_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ArcFileSystemWatcherService::FileSystemWatcher::~FileSystemWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ArcFileSystemWatcherService::FileSystemWatcher::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Initialize with the current timestamp map and avoid initial notification.
  // It is not needed since MediaProvider scans whole storage area on boot.
  last_notify_time_ = base::TimeTicks::Now();
  last_timestamp_map_ =
      BuildTimestampMap(cros_dir_, android_dir_);

  watcher_ = std::make_unique<base::FilePathWatcher>();
  // Check whether inotify limit is hit in |cros_dir_| in the first place.
  if (!watcher_->Watch(
          cros_dir_, base::FilePathWatcher::Type::kRecursive,
          base::BindRepeating(&FileSystemWatcher::OnFilePathChanged,
                              weak_ptr_factory_.GetWeakPtr()))) {
    LOG(WARNING)
        << "Failed to start FileSystemWatcher for " << cros_dir_
        << " because the number of required inotify watches exceeded its limit";
  }
}

void ArcFileSystemWatcherService::FileSystemWatcher::OnFilePathChanged(
    const base::FilePath& path,
    bool error) {
  // On Linux, |error| indicates whether inotify exceeds its limit during
  // FileSystemWatcher::OnFilePathChanged(). Also, |path| is always the same
  // path as one given to FilePathWatcher::Watch().
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check whether inotify limit is hit.
  if (error) {
    LOG(WARNING)
        << "The watcher won't be notified of subsequent filesystem changes in "
        << cros_dir_
        << " because the number of required inotify watches exceeded its limit";
    return;
  }
  if (!outstanding_task_) {
    outstanding_task_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FileSystemWatcher::DelayBuildTimestampMap,
                       weak_ptr_factory_.GetWeakPtr()),
        kBuildTimestampMapDelay);
  } else {
    last_notify_time_ = base::TimeTicks::Now();
  }
}

void ArcFileSystemWatcherService::FileSystemWatcher::DelayBuildTimestampMap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(outstanding_task_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BuildTimestampMapCallback, cros_dir_, android_dir_),
      base::BindOnce(&FileSystemWatcher::OnBuildTimestampMap,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcFileSystemWatcherService::FileSystemWatcher::OnBuildTimestampMap(
    std::pair<base::TimeTicks, TimestampMap> timestamp_and_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(outstanding_task_);
  base::TimeTicks snapshot_time = timestamp_and_map.first;
  TimestampMap current_timestamp_map = std::move(timestamp_and_map.second);
  std::vector<base::FilePath> changed_paths =
      CollectChangedPaths(last_timestamp_map_, current_timestamp_map);

  last_timestamp_map_ = std::move(current_timestamp_map);

  std::vector<std::string> string_paths(changed_paths.size());
  for (size_t i = 0; i < changed_paths.size(); ++i) {
    string_paths[i] = changed_paths[i].value();
    // Files inside kAndroidMyFilesDownloadsDir are skipped by Android's
    // MediaScanner in order to avoid duplicate indexing. They should be indexed
    // as files inside kAndroidDownloadDir.
    base::ReplaceFirstSubstringAfterOffset(
        &string_paths[i], 0, kAndroidMyFilesDownloadsDir, kAndroidDownloadDir);
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(callback_, std::move(string_paths)));
  if (last_notify_time_ > snapshot_time)
    DelayBuildTimestampMap();
  else
    outstanding_task_ = false;
}

// static
ArcFileSystemWatcherService* ArcFileSystemWatcherService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcFileSystemWatcherServiceFactory::GetForBrowserContext(context);
}

ArcFileSystemWatcherService::ArcFileSystemWatcherService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : context_(context),
      arc_bridge_service_(bridge_service),
      file_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  arc_bridge_service_->file_system()->AddObserver(this);
  ArcVolumeMounterBridge::GetForBrowserContext(context_)->Initialize(this);
}

ArcFileSystemWatcherService::~ArcFileSystemWatcherService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StopWatchingFileSystem();
  DCHECK(removable_media_watchers_.empty());
  DCHECK(!myfiles_watcher_);

  arc_bridge_service_->file_system()->RemoveObserver(this);
}

void ArcFileSystemWatcherService::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWatchingFileSystem();
  StartWatchingFileSystem();
}

void ArcFileSystemWatcherService::OnConnectionClosed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWatchingFileSystem();
}

void ArcFileSystemWatcherService::StartWatchingFileSystem() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!myfiles_watcher_);

  // Attach a watcher to MyFiles and trigger SendAllMountEvents().
  Profile* profile = Profile::FromBrowserContext(context_);
  myfiles_watcher_ = CreateAndStartFileSystemWatcher(
      file_manager::util::GetMyFilesFolderForProfile(profile),
      base::FilePath(kAndroidMyFilesDir),
      base::BindOnce(&ArcFileSystemWatcherService::OnMyFilesWatcherStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcFileSystemWatcherService::StopWatchingFileSystem() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  watching_file_system_changes_ = false;

  for (auto& watcher : removable_media_watchers_) {
    file_task_runner_->DeleteSoon(FROM_HERE, watcher.second.release());
  }
  removable_media_watchers_.clear();

  file_task_runner_->DeleteSoon(FROM_HERE, myfiles_watcher_.release());
}

void ArcFileSystemWatcherService::OnMyFilesWatcherStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(myfiles_watcher_);
  watching_file_system_changes_ = true;
  TriggerSendAllMountEvents();
}

std::unique_ptr<ArcFileSystemWatcherService::FileSystemWatcher>
ArcFileSystemWatcherService::CreateAndStartFileSystemWatcher(
    const base::FilePath& cros_path,
    const base::FilePath& android_path,
    base::OnceClosure callback) {
  auto watcher = std::make_unique<FileSystemWatcher>(
      base::BindRepeating(&ArcFileSystemWatcherService::OnFileSystemChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      cros_path, android_path);

  file_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&FileSystemWatcher::Start,
                     base::Unretained(watcher.get())),
      std::move(callback));
  return watcher;
}

void ArcFileSystemWatcherService::OnFileSystemChanged(
    const std::vector<std::string>& paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), RequestMediaScan);
  if (!instance)
    return;

  instance->RequestMediaScan(paths);
}

bool ArcFileSystemWatcherService::IsWatchingFileSystemChanges() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return watching_file_system_changes_;
}

void ArcFileSystemWatcherService::StartWatchingRemovableMedia(
    const std::string& fs_uuid,
    const std::string& mount_path,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Make sure that there is no removable media entry. Otherwise, the
  // map assignment will remove the entry after a new entry is
  // created, possibly causing crash if there is 2 mount events without
  // unmounting events in between.
  if (removable_media_watchers_.count(mount_path)) {
    std::move(callback).Run();
    return;
  }

  // Make sure the callback is triggered after the file system is attached in
  // file_task_runner.
  base::FilePath android_path =
      base::FilePath(kAndroidStorageDir).Append(fs_uuid);
  removable_media_watchers_[mount_path] = CreateAndStartFileSystemWatcher(
      base::FilePath(mount_path), android_path, std::move(callback));
}

void ArcFileSystemWatcherService::StopWatchingRemovableMedia(
    const std::string& mount_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!removable_media_watchers_.count(mount_path)) {
    VLOG(1) << "Unmounting non-existing volume with mount path: " << mount_path;
    return;
  }
  file_task_runner_->DeleteSoon(
      FROM_HERE, removable_media_watchers_[mount_path].release());
  removable_media_watchers_.erase(mount_path);
}

void ArcFileSystemWatcherService::TriggerSendAllMountEvents() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ArcVolumeMounterBridge::GetForBrowserContext(context_)->SendAllMountEvents();
}

// static
void ArcFileSystemWatcherService::EnsureFactoryBuilt() {
  ArcFileSystemWatcherServiceFactory::GetInstance();
}

}  // namespace arc
