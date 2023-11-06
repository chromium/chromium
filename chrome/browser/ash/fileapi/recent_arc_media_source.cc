// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_arc_media_source.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <utility>

#include "ash/components/arc/mojom/file_system.mojom.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

using content::BrowserThread;

namespace ash {

namespace {

const char kAndroidDownloadDirPrefix[] = "/storage/emulated/0/Download/";
// The path of the MyFiles directory inside Android. The UUID "0000....2019" is
// defined in ash/components/arc/volume_mounter/arc_volume_mounter_bridge.cc.
// TODO(crbug.com/929031): Move MyFiles constants to a common place.
const char kAndroidMyFilesDirPrefix[] =
    "/storage/0000000000000000000000000000CAFEF00D2019/";

// Android's MediaDocumentsProvider.queryRecentDocuments() doesn't support
// audio files, http://b/175155820
const char* kMediaDocumentsProviderRootIds[] = {
    arc::kImagesRootDocumentId,
    arc::kVideosRootDocumentId,
    arc::kDocumentsRootDocumentId,
};

base::FilePath GetRelativeMountPath(const std::string& root_id) {
  base::FilePath mount_path =
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         // In MediaDocumentsProvider, |root_id|
                                         // and |root_document_id| are the same.
                                         root_id);
  base::FilePath relative_mount_path;
  base::FilePath(arc::kDocumentsProviderMountPointPath)
      .AppendRelativePath(mount_path, &relative_mount_path);
  return relative_mount_path;
}

bool IsInsideDownloadsOrMyFiles(const std::string& path) {
  if (base::StartsWith(path, kAndroidDownloadDirPrefix,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
  if (base::StartsWith(path, kAndroidMyFilesDirPrefix,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
  return false;
}

}  // namespace

const char RecentArcMediaSource::kLoadHistogramName[] =
    "FileBrowser.Recent.LoadArcMedia";

// Handles GetRecentFiles() for a root in MediaDocumentsProvider.
//
// It gathers recent files in following steps:
//
// 1. Call ArcFileSystemOperationRunner::GetRecentDocuments() to get the
//    list of IDs of recently modified documents.
//
// 2. Call ArcDocumentsProviderRoot::ReadDirectory() recursively to
//    look for file paths of recently modified documents on Media View.
//
// 3. After the whole tree is scanned, build FileSystemURLs for paths
//    found and return them.
//
class RecentArcMediaSource::MediaRoot {
 public:
  MediaRoot(const std::string& root_id, Profile* profile);

  MediaRoot(const MediaRoot&) = delete;
  MediaRoot& operator=(const MediaRoot&) = delete;

  ~MediaRoot();

  void GetRecentFiles(Params params);

 private:
  void OnGetRecentDocuments(
      absl::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents);
  void ScanDirectory(const base::FilePath& path);
  void OnReadDirectory(
      const base::FilePath& path,
      base::File::Error result,
      std::vector<arc::ArcDocumentsProviderRoot::ThinFileInfo> files);
  void OnComplete();

  storage::FileSystemURL BuildDocumentsProviderUrl(
      const base::FilePath& path) const;
  bool MatchesFileType(FileType file_type) const;

  // Set in the constructor.
  const std::string root_id_;
  const raw_ptr<Profile, ExperimentalAsh> profile_;
  const base::FilePath relative_mount_path_;

  // Set at the beginning of GetRecentFiles().
  absl::optional<Params> params_;

  // Number of in-flight ReadDirectory() calls by ScanDirectory().
  int num_inflight_readdirs_ = 0;

  // Maps a document ID to a RecentFile.
  // In OnGetRecentDocuments(), this map is initialized with document IDs
  // returned by GetRecentDocuments(), and its values are filled as we scan the
  // tree in ScanDirectory().
  // In case of multiple files with the same document ID found, the file with
  // lexicographically smallest URL is kept. A nullopt value means the
  // corresponding file is not (yet) found.
  std::map<std::string, absl::optional<RecentFile>> document_id_to_file_;

  base::WeakPtrFactory<MediaRoot> weak_ptr_factory_{this};
};

RecentArcMediaSource::MediaRoot::MediaRoot(const std::string& root_id,
                                           Profile* profile)
    : root_id_(root_id),
      profile_(profile),
      relative_mount_path_(GetRelativeMountPath(root_id)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentArcMediaSource::MediaRoot::~MediaRoot() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RecentArcMediaSource::MediaRoot::GetRecentFiles(Params params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!params_.has_value());
  DCHECK_EQ(0, num_inflight_readdirs_);
  DCHECK(document_id_to_file_.empty());

  params_.emplace(std::move(params));

  auto* runner =
      arc::ArcFileSystemOperationRunner::GetForBrowserContext(profile_);
  if (!runner) {
    // This happens when ARC is not allowed in this profile.
    OnComplete();
    return;
  }

  if (!MatchesFileType(params_.value().file_type())) {
    // Return immediately without results when this root's id does not match the
    // requested file type.
    OnComplete();
    return;
  }

  runner->GetRecentDocuments(arc::kMediaDocumentsProviderAuthority, root_id_,
                             base::BindOnce(&MediaRoot::OnGetRecentDocuments,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void RecentArcMediaSource::MediaRoot::OnGetRecentDocuments(
    absl::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());
  DCHECK_EQ(0, num_inflight_readdirs_);
  DCHECK(document_id_to_file_.empty());

  const std::u16string q16 = base::UTF8ToUTF16(params_->query());
  // Initialize |document_id_to_file_| with recent document IDs returned.
  if (maybe_documents.has_value()) {
    for (const auto& document : maybe_documents.value()) {
      // Exclude media files under Downloads or MyFiles directory since they are
      // covered by RecentDiskSource.
      if (document->android_file_system_path.has_value() &&
          IsInsideDownloadsOrMyFiles(
              document->android_file_system_path.value())) {
        continue;
      }
      if (!FileNameMatches(base::UTF8ToUTF16(document->display_name), q16)) {
        continue;
      }
      document_id_to_file_.emplace(document->document_id, absl::nullopt);
    }
  }

  if (document_id_to_file_.empty()) {
    OnComplete();
    return;
  }

  // We have several recent documents, so start searching their real paths.
  ScanDirectory(base::FilePath());
}

void RecentArcMediaSource::MediaRoot::ScanDirectory(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());

  ++num_inflight_readdirs_;

  auto* root_map =
      arc::ArcDocumentsProviderRootMap::GetForBrowserContext(profile_);
  if (!root_map) {
    // We already checked ARC is allowed for this profile (indirectly), so
    // this should never happen.
    LOG(ERROR) << "ArcDocumentsProviderRootMap is not available";
    OnReadDirectory(path, base::File::FILE_ERROR_FAILED, {});
    return;
  }

  // In MediaDocumentsProvider, |root_id| and |root_document_id| are the same.
  auto* root =
      root_map->Lookup(arc::kMediaDocumentsProviderAuthority, root_id_);
  if (!root) {
    // Media roots should always exist.
    LOG(ERROR) << "ArcDocumentsProviderRoot is missing";
    OnReadDirectory(path, base::File::FILE_ERROR_NOT_FOUND, {});
    return;
  }

  root->ReadDirectory(
      path, base::BindOnce(&RecentArcMediaSource::MediaRoot::OnReadDirectory,
                           weak_ptr_factory_.GetWeakPtr(), path));
}

void RecentArcMediaSource::MediaRoot::OnReadDirectory(
    const base::FilePath& path,
    base::File::Error result,
    std::vector<arc::ArcDocumentsProviderRoot::ThinFileInfo> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());

  for (const auto& file : files) {
    base::FilePath subpath = path.Append(file.name);
    if (file.is_directory) {
      if (!params_->IsLate()) {
        ScanDirectory(subpath);
      }
      continue;
    }

    auto iter = document_id_to_file_.find(file.document_id);
    if (iter == document_id_to_file_.end()) {
      continue;
    }

    // Update |document_id_to_file_|.
    // We keep the lexicographically smallest URL to stabilize the results when
    // there are multiple files with the same document ID.
    auto url = BuildDocumentsProviderUrl(subpath);
    absl::optional<RecentFile>& entry = iter->second;
    if (!entry.has_value() ||
        storage::FileSystemURL::Comparator()(url, entry.value().url())) {
      entry = RecentFile(url, file.last_modified);
    }
  }

  --num_inflight_readdirs_;
  DCHECK_LE(0, num_inflight_readdirs_);

  if (num_inflight_readdirs_ == 0) {
    OnComplete();
  }
}

void RecentArcMediaSource::MediaRoot::OnComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());
  DCHECK_EQ(0, num_inflight_readdirs_);

  std::vector<RecentFile> files;
  for (const auto& entry : document_id_to_file_) {
    const absl::optional<RecentFile>& file = entry.second;
    if (file.has_value()) {
      files.emplace_back(file.value());
    }
  }
  document_id_to_file_.clear();

  Params params = std::move(params_.value());
  params_.reset();
  std::move(params.callback()).Run(std::move(files));
}

storage::FileSystemURL
RecentArcMediaSource::MediaRoot::BuildDocumentsProviderUrl(
    const base::FilePath& path) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  return mount_points->CreateExternalFileSystemURL(
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(params_.value().origin())),
      arc::kDocumentsProviderMountPointName, relative_mount_path_.Append(path));
}

bool RecentArcMediaSource::MediaRoot::MatchesFileType(
    FileType file_type) const {
  switch (file_type) {
    case FileType::kAll:
      return true;
    case FileType::kImage:
      return root_id_ == arc::kImagesRootDocumentId;
    case FileType::kVideo:
      return root_id_ == arc::kVideosRootDocumentId;
    case FileType::kDocument:
      return root_id_ == arc::kDocumentsRootDocumentId;
    default:
      return false;
  }
}

RecentArcMediaSource::RecentArcMediaSource(Profile* profile)
    : profile_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const char* root_id : kMediaDocumentsProviderRootIds) {
    roots_.emplace_back(std::make_unique<MediaRoot>(root_id, profile_));
  }
}

RecentArcMediaSource::~RecentArcMediaSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RecentArcMediaSource::GetRecentFiles(Params params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!params_.has_value());
  DCHECK(build_start_time_.is_null());
  DCHECK_EQ(0, num_inflight_roots_);
  DCHECK(files_.empty());

  // If ARC file system operations will be deferred, return immediately without
  // recording UMA metrics.
  //
  // TODO(nya): Return files progressively rather than simply giving up.
  // Also, it is wrong to assume all following operations will not be deferred
  // just because this function returned true. However, in practice, it is rare
  // ArcFileSystemOperationRunner's deferring state switches from disabled to
  // enabled (one such case is when ARC container crashes).
  if (!WillArcFileSystemOperationsRunImmediately()) {
    std::move(params.callback()).Run({});
    return;
  }

  params_.emplace(std::move(params));

  build_start_time_ = base::TimeTicks::Now();

  num_inflight_roots_ = roots_.size();
  if (num_inflight_roots_ == 0) {
    OnComplete();
    return;
  }

  for (auto& root : roots_) {
    root->GetRecentFiles(
        Params(params_.value().file_system_context(), params_.value().origin(),
               params_.value().max_files(), params_.value().query(),
               params_.value().cutoff_time(), params_.value().end_time(),
               params_.value().file_type(),
               base::BindOnce(&RecentArcMediaSource::OnGetRecentFilesForRoot,
                              weak_ptr_factory_.GetWeakPtr())));
  }
}

void RecentArcMediaSource::OnGetRecentFilesForRoot(
    std::vector<RecentFile> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());

  files_.insert(files_.end(), std::make_move_iterator(files.begin()),
                std::make_move_iterator(files.end()));

  --num_inflight_roots_;
  if (num_inflight_roots_ == 0) {
    OnComplete();
  }
}

void RecentArcMediaSource::OnComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());
  DCHECK(!build_start_time_.is_null());
  DCHECK_EQ(0, num_inflight_roots_);

  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - build_start_time_);
  build_start_time_ = base::TimeTicks();

  Params params = std::move(params_.value());
  params_.reset();
  std::vector<RecentFile> files = std::move(files_);
  files_.clear();
  std::move(params.callback()).Run(std::move(files));
}

bool RecentArcMediaSource::WillArcFileSystemOperationsRunImmediately() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* runner =
      arc::ArcFileSystemOperationRunner::GetForBrowserContext(profile_);

  // If ARC is not allowed the user, |runner| is nullptr.
  if (!runner) {
    return false;
  }

  return !runner->WillDefer();
}

}  // namespace ash
