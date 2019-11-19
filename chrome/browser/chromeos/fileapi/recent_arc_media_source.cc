// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_arc_media_source.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const char kAndroidDownloadDirPrefix[] = "/storage/emulated/0/Download/";

const char kMediaDocumentsProviderAuthority[] =
    "com.android.providers.media.documents";
const char* kMediaDocumentsProviderRootIds[] = {
    "images_root", "videos_root",
};

base::FilePath GetRelativeMountPath(const std::string& root_id) {
  base::FilePath mount_path = arc::GetDocumentsProviderMountPath(
      kMediaDocumentsProviderAuthority,
      // In MediaDocumentsProvider, |root_id| and |root_document_id| are
      // the same.
      root_id);
  base::FilePath relative_mount_path;
  base::FilePath(arc::kDocumentsProviderMountPointPath)
      .AppendRelativePath(mount_path, &relative_mount_path);
  return relative_mount_path;
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
  ~MediaRoot();

  void GetRecentFiles(Params params);

 private:
  void OnGetRecentDocuments(
      base::Optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents);
  void ScanDirectory(const base::FilePath& path);
  void OnReadDirectory(
      const base::FilePath& path,
      base::File::Error result,
      std::vector<arc::ArcDocumentsProviderRoot::ThinFileInfo> files);
  void OnComplete();

  storage::FileSystemURL BuildDocumentsProviderUrl(
      const base::FilePath& path) const;

  // Set in the constructor.
  const std::string root_id_;
  Profile* const profile_;
  const base::FilePath relative_mount_path_;

  // Set at the beginning of GetRecentFiles().
  base::Optional<Params> params_;

  // Number of in-flight ReadDirectory() calls by ScanDirectory().
  int num_inflight_readdirs_ = 0;

  // Maps a document ID to a RecentFile.
  // In OnGetRecentDocuments(), this map is initialized with document IDs
  // returned by GetRecentDocuments(), and its values are filled as we scan the
  // tree in ScanDirectory().
  // In case of multiple files with the same document ID found, the file with
  // lexicographically smallest URL is kept. A nullopt value means the
  // corresponding file is not (yet) found.
  std::map<std::string, base::Optional<RecentFile>> document_id_to_file_;

  base::WeakPtrFactory<MediaRoot> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaRoot);
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

  runner->GetRecentDocuments(kMediaDocumentsProviderAuthority, root_id_,
                             base::Bind(&MediaRoot::OnGetRecentDocuments,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void RecentArcMediaSource::MediaRoot::OnGetRecentDocuments(
    base::Optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());
  DCHECK_EQ(0, num_inflight_readdirs_);
  DCHECK(document_id_to_file_.empty());

  // Initialize |document_id_to_file_| with recent document IDs returned.
  if (maybe_documents.has_value()) {
    for (const auto& document : maybe_documents.value()) {
      // Exclude media files under Downloads directory since they are covered
      // by RecentDownloadSource.
      if (document->android_file_system_path.has_value() &&
          base::StartsWith(document->android_file_system_path.value(),
                           kAndroidDownloadDirPrefix,
                           base::CompareCase::SENSITIVE))
        continue;

      document_id_to_file_.emplace(document->document_id, base::nullopt);
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
  auto* root = root_map->Lookup(kMediaDocumentsProviderAuthority, root_id_);
  if (!root) {
    // Media roots should always exist.
    LOG(ERROR) << "ArcDocumentsProviderRoot is missing";
    OnReadDirectory(path, base::File::FILE_ERROR_NOT_FOUND, {});
    return;
  }

  root->ReadDirectory(
      path, base::Bind(&RecentArcMediaSource::MediaRoot::OnReadDirectory,
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
      ScanDirectory(subpath);
      continue;
    }

    auto iter = document_id_to_file_.find(file.document_id);
    if (iter == document_id_to_file_.end())
      continue;

    // Update |document_id_to_file_|.
    // We keep the lexicographically smallest URL to stabilize the results when
    // there are multiple files with the same document ID.
    auto url = BuildDocumentsProviderUrl(subpath);
    base::Optional<RecentFile>& entry = iter->second;
    if (!entry.has_value() ||
        storage::FileSystemURL::Comparator()(url, entry.value().url()))
      entry = RecentFile(url, file.last_modified);
  }

  --num_inflight_readdirs_;
  DCHECK_LE(0, num_inflight_readdirs_);

  if (num_inflight_readdirs_ == 0)
    OnComplete();
}

void RecentArcMediaSource::MediaRoot::OnComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params_.has_value());
  DCHECK_EQ(0, num_inflight_readdirs_);

  std::vector<RecentFile> files;
  for (const auto& entry : document_id_to_file_) {
    const base::Optional<RecentFile>& file = entry.second;
    if (file.has_value())
      files.emplace_back(file.value());
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
      params_.value().origin(), arc::kDocumentsProviderMountPointName,
      relative_mount_path_.Append(path));
}

RecentArcMediaSource::RecentArcMediaSource(Profile* profile)
    : profile_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const char* root_id : kMediaDocumentsProviderRootIds)
    roots_.emplace_back(std::make_unique<MediaRoot>(root_id, profile_));
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
               params_.value().max_files(), params_.value().cutoff_time(),
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
  if (num_inflight_roots_ == 0)
    OnComplete();
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
  if (!runner)
    return false;

  return !runner->WillDefer();
}

}  // namespace chromeos
