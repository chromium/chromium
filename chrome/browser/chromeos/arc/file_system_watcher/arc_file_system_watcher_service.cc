// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/file_system_watcher/arc_file_system_watcher_service.h"

#include <string.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/singleton.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// Mapping from Android file paths to last modified timestamps.
using TimestampMap = std::map<base::FilePath, base::Time>;

namespace arc {

namespace {

// The Downloads path inside ARC container. This will be the path that
// is used in MediaScanner.scanFile request.
constexpr base::FilePath::CharType kAndroidDownloadDir[] =
    FILE_PATH_LITERAL("/storage/emulated/0/Download");

// The removable media path in ChromeOS. This is the actual directory to be
// watched.
constexpr base::FilePath::CharType kCrosRemovableMediaDir[] =
    FILE_PATH_LITERAL("/media/removable");

// The removable media path inside ARC container. This will be the path that
// is used in MediaScanner.scanFile request. The /var is mandatory here because
// it is the ARC container side path name.
constexpr base::FilePath::CharType kAndroidRemovableMediaDir[] =
    FILE_PATH_LITERAL("/var/run/arc/media/removable");

// How long to wait for new inotify events before building the updated timestamp
// map.
const base::TimeDelta kBuildTimestampMapDelay =
    base::TimeDelta::FromMilliseconds(1000);

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
    // Skip non-media files for efficiency.
    if (!HasAndroidSupportedMediaExtension(cros_path))
      continue;
    // Android file path can be obtained by replacing |cros_dir|
    // prefix with |android_dir|.
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

  static ArcFileSystemWatcherServiceFactory* GetInstance() {
    return base::Singleton<ArcFileSystemWatcherServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcFileSystemWatcherServiceFactory>;
  ArcFileSystemWatcherServiceFactory() = default;
  ~ArcFileSystemWatcherServiceFactory() override = default;
};

}  // namespace

// The set of media file extensions supported by Android MediaScanner.
// Entries must be lower-case and sorted by lexicographical order for
// binary search.
//
// The current list was taken from aosp-nougat version of
// frameworks/base/media/java/android/media/MediaFile.java, and included
// file type categories are:
// - Audio
// - MIDI
// - Video
// - Image
// - Raw image
const char* kAndroidSupportedMediaExtensions[] = {
    ".3g2",    // FILE_TYPE_3GPP2, video/3gpp2
    ".3gp",    // FILE_TYPE_3GPP, video/3gpp
    ".3gpp",   // FILE_TYPE_3GPP, video/3gpp
    ".3gpp2",  // FILE_TYPE_3GPP2, video/3gpp2
    ".aac",    // FILE_TYPE_AAC, audio/aac, audio/aac-adts
    ".amr",    // FILE_TYPE_AMR, audio/amr
    ".arw",    // FILE_TYPE_ARW, image/x-sony-arw
    ".asf",    // FILE_TYPE_ASF, video/x-ms-asf
    ".avi",    // FILE_TYPE_AVI, video/avi
    ".awb",    // FILE_TYPE_AWB, audio/amr-wb
    ".bmp",    // FILE_TYPE_BMP, image/x-ms-bmp
    ".cr2",    // FILE_TYPE_CR2, image/x-canon-cr2
    ".dng",    // FILE_TYPE_DNG, image/x-adobe-dng
    ".fl",     // FILE_TYPE_FL, application/x-android-drm-fl
    ".gif",    // FILE_TYPE_GIF, image/gif
    ".imy",    // FILE_TYPE_IMY, audio/imelody
    ".jpeg",   // FILE_TYPE_JPEG, image/jpeg
    ".jpg",    // FILE_TYPE_JPEG, image/jpeg
    ".m4a",    // FILE_TYPE_M4A, audio/mp4
    ".m4v",    // FILE_TYPE_M4V, video/mp4
    ".mid",    // FILE_TYPE_MID, audio/midi
    ".midi",   // FILE_TYPE_MID, audio/midi
    ".mka",    // FILE_TYPE_MKA, audio/x-matroska
    ".mkv",    // FILE_TYPE_MKV, video/x-matroska
    ".mov",    // FILE_TYPE_QT, video/quicktime
    ".mp3",    // FILE_TYPE_MP3, audio/mpeg
    ".mp4",    // FILE_TYPE_MP4, video/mp4
    ".mpeg",   // FILE_TYPE_MP4, video/mpeg, video/mp2p
    ".mpg",    // FILE_TYPE_MP4, video/mpeg, video/mp2p
    ".mpga",   // FILE_TYPE_MP3, audio/mpeg
    ".mxmf",   // FILE_TYPE_MID, audio/midi
    ".nef",    // FILE_TYPE_NEF, image/x-nikon-nef
    ".nrw",    // FILE_TYPE_NRW, image/x-nikon-nrw
    ".oga",    // FILE_TYPE_OGG, application/ogg
    ".ogg",    // FILE_TYPE_OGG, audio/ogg, application/ogg
    ".orf",    // FILE_TYPE_ORF, image/x-olympus-orf
    ".ota",    // FILE_TYPE_MID, audio/midi
    ".pef",    // FILE_TYPE_PEF, image/x-pentax-pef
    ".png",    // FILE_TYPE_PNG, image/png
    ".raf",    // FILE_TYPE_RAF, image/x-fuji-raf
    ".rtttl",  // FILE_TYPE_MID, audio/midi
    ".rtx",    // FILE_TYPE_MID, audio/midi
    ".rw2",    // FILE_TYPE_RW2, image/x-panasonic-rw2
    ".smf",    // FILE_TYPE_SMF, audio/sp-midi
    ".srw",    // FILE_TYPE_SRW, image/x-samsung-srw
    ".ts",     // FILE_TYPE_MP2TS, video/mp2ts
    ".wav",    // FILE_TYPE_WAV, audio/x-wav
    ".wbmp",   // FILE_TYPE_WBMP, image/vnd.wap.wbmp
    ".webm",   // FILE_TYPE_WEBM, video/webm
    ".webp",   // FILE_TYPE_WEBP, image/webp
    ".wma",    // FILE_TYPE_WMA, audio/x-ms-wma
    ".wmv",    // FILE_TYPE_WMV, video/x-ms-wmv
    ".xmf",    // FILE_TYPE_MID, audio/midi
};
const int kAndroidSupportedMediaExtensionsSize =
    arraysize(kAndroidSupportedMediaExtensions);

bool HasAndroidSupportedMediaExtension(const base::FilePath& path) {
  const std::string extension = base::ToLowerASCII(path.Extension());
  const auto less_comparator = [](const char* a, const char* b) {
    return strcmp(a, b) < 0;
  };
  DCHECK(std::is_sorted(
      kAndroidSupportedMediaExtensions,
      kAndroidSupportedMediaExtensions + kAndroidSupportedMediaExtensionsSize,
      less_comparator));
  return std::binary_search(
      kAndroidSupportedMediaExtensions,
      kAndroidSupportedMediaExtensions + kAndroidSupportedMediaExtensionsSize,
      extension.c_str(), less_comparator);
}

// The core part of ArcFileSystemWatcherService to watch for file changes in
// directory.
class ArcFileSystemWatcherService::FileSystemWatcher {
 public:
  using Callback = base::Callback<void(const std::vector<std::string>& paths)>;

  FileSystemWatcher(content::BrowserContext* context,
                    const Callback& callback,
                    base::FilePath cros_dir,
                    base::FilePath android_dir);
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
  base::FilePath cros_dir_;
  base::FilePath android_dir_;
  std::unique_ptr<base::FilePathWatcher> watcher_;
  TimestampMap last_timestamp_map_;
  // The timestamp of the last OnFilePathChanged callback received.
  base::TimeTicks last_notify_time_;
  // Whether or not there is an outstanding task to update last_timestamp_map_.
  bool outstanding_task_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileSystemWatcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemWatcher);
};

ArcFileSystemWatcherService::FileSystemWatcher::FileSystemWatcher(
    content::BrowserContext* context,
    const Callback& callback,
    base::FilePath cros_dir,
    base::FilePath android_dir)
    : callback_(callback),
      cros_dir_(cros_dir),
      android_dir_(android_dir),
      last_notify_time_(base::TimeTicks()),
      outstanding_task_(false),
      weak_ptr_factory_(this) {
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
  // On Linux, base::FilePathWatcher::Watch() always returns true.
  watcher_->Watch(cros_dir_, true,
                  base::Bind(&FileSystemWatcher::OnFilePathChanged,
                             weak_ptr_factory_.GetWeakPtr()));
}

void ArcFileSystemWatcherService::FileSystemWatcher::OnFilePathChanged(
    const base::FilePath& path,
    bool error) {
  // On Linux, |error| is always false. Also, |path| is always the same path
  // as one given to FilePathWatcher::Watch().
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!outstanding_task_) {
    outstanding_task_ = true;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
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
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::Bind(&BuildTimestampMapCallback, cros_dir_,
                 android_dir_),
      base::Bind(&FileSystemWatcher::OnBuildTimestampMap,
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
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback_, std::move(string_paths)));
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
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()})),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  arc_bridge_service_->file_system()->AddObserver(this);
}

ArcFileSystemWatcherService::~ArcFileSystemWatcherService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StopWatchingFileSystem();
  DCHECK(!downloads_watcher_);
  DCHECK(!removable_media_watcher_);

  arc_bridge_service_->file_system()->RemoveObserver(this);
}

void ArcFileSystemWatcherService::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartWatchingFileSystem();
}

void ArcFileSystemWatcherService::OnConnectionClosed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWatchingFileSystem();
}

void ArcFileSystemWatcherService::StartWatchingFileSystem() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWatchingFileSystem();
  DCHECK(!downloads_watcher_);
  downloads_watcher_ = std::make_unique<FileSystemWatcher>(
      context_,
      base::Bind(&ArcFileSystemWatcherService::OnFileSystemChanged,
                 weak_ptr_factory_.GetWeakPtr()),
      DownloadPrefs(Profile::FromBrowserContext(context_))
          .GetDefaultDownloadDirectoryForProfile()
          .StripTrailingSeparators(),
      base::FilePath(kAndroidDownloadDir));
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemWatcher::Start,
                                base::Unretained(downloads_watcher_.get())));
  DCHECK(!removable_media_watcher_);
  removable_media_watcher_ = std::make_unique<FileSystemWatcher>(
      context_,
      base::Bind(&ArcFileSystemWatcherService::OnFileSystemChanged,
                 weak_ptr_factory_.GetWeakPtr()),
      base::FilePath(kCrosRemovableMediaDir),
      base::FilePath(kAndroidRemovableMediaDir));
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemWatcher::Start,
                     base::Unretained(removable_media_watcher_.get())));
}

void ArcFileSystemWatcherService::StopWatchingFileSystem() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (downloads_watcher_)
    file_task_runner_->DeleteSoon(FROM_HERE, downloads_watcher_.release());
  if (removable_media_watcher_)
    file_task_runner_->DeleteSoon(FROM_HERE,
                                  removable_media_watcher_.release());
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

}  // namespace arc
