// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_arc_media_source.h"

#include <sys/types.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/components/arc/mojom/file_system.mojom.h"
#include "base/containers/extend.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"
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
    arc::kImagesRootId,
    arc::kVideosRootId,
    arc::kDocumentsRootId,
};

base::FilePath GetRelativeMountPath(const std::string& root_id) {
  base::FilePath mount_path =
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
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

// Tidies up the vector of files by sorting them and limiting their number to
// the specified maximum.
std::vector<RecentFile> PrepareResponse(std::vector<RecentFile>&& files,
                                        size_t max_files) {
  std::sort(files.begin(), files.end(), RecentFileComparator());
  if (files.size() > max_files) {
    files.resize(max_files);
  }
  return files;
}

}  // namespace

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

  void GetRecentFiles(Params params, GetRecentFilesCallback callback);

  // Stops the execution of the document retrieval for this root and returns
  // any RecentFiles found so far.
  std::vector<RecentFile> Stop(int32_t call_id);

  // Sets lag for this particular root. This will cause the root to wait
  // the specified lag before delivering results on the callback specified
  // as the parameter of GetRecentFiles method.
  void SetLagForTesting(base::TimeDelta lag) { lag_ = lag; }

 private:
  // Extra method that allows us to insert an optional lag between the runner
  // being done and the OnGotRecentDocuments being called.
  void OnRunnerDone(
      const Params& params,
      std::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents);

  // The method called once recent document pointers have been retrieved. This
  // may take place immediately after the runner was done, or with a small lag
  // that helps testing the interaction with the Stop method.
  void OnGotRecentDocuments(
      const Params& params,
      std::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents);

  void ScanDirectory(const Params& params, const base::FilePath& path);

  void OnReadDirectory(
      const Params& params,
      const base::FilePath& path,
      base::File::Error result,
      std::vector<arc::ArcDocumentsProviderRoot::ThinFileInfo> files);

  void OnComplete(int32_t call_id);

  storage::FileSystemURL BuildDocumentsProviderUrl(
      const Params& params,
      const base::FilePath& path) const;

  bool MatchesFileType(FileType file_type) const;

  // Similar to RecentArcMediaSource a context for the GetRecentFiles call for
  // this root.
  struct CallContext {
    explicit CallContext(GetRecentFilesCallback callback);
    CallContext(CallContext&& context);
    ~CallContext();

    // The callback to call if we complete the scan before the Stop method
    // is called.
    GetRecentFilesCallback callback;

    // Number of in-flight ReadDirectory() calls by ScanDirectory().
    int num_inflight_readdirs = 0;

    // Maps a document ID to a RecentFile.
    // In OnGotRecentDocuments(), this map is initialized with document IDs
    // returned by GetRecentDocuments(), and its values are filled as we scan
    // the tree in ScanDirectory().
    // In case of multiple files with the same document ID found, the file with
    // lexicographically smallest URL is kept. A nullopt value means the
    // corresponding file is not (yet) found.
    std::map<std::string, std::optional<RecentFile>> document_id_to_file;
  };

  // Set in the constructor.
  const std::string root_id_;
  const raw_ptr<Profile> profile_;
  const base::FilePath relative_mount_path_;

  // A map from the call ID to the call context.
  base::IDMap<std::unique_ptr<CallContext>> context_map_;

  // The artificial lag introduced to this root for test purposes.
  base::TimeDelta lag_;

  // Timer; only allocated if the lag is positive.
  std::unique_ptr<base::OneShotTimer> timer_;

  base::WeakPtrFactory<MediaRoot> weak_ptr_factory_{this};
};

RecentArcMediaSource::MediaRoot::CallContext::CallContext(
    GetRecentFilesCallback callback)
    : callback(std::move(callback)) {}
RecentArcMediaSource::MediaRoot::CallContext::CallContext(CallContext&& context)
    : callback(std::move(context.callback)),
      num_inflight_readdirs(context.num_inflight_readdirs),
      document_id_to_file(std::move(context.document_id_to_file)) {}

RecentArcMediaSource::MediaRoot::CallContext::~CallContext() = default;

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

void RecentArcMediaSource::MediaRoot::GetRecentFiles(
    Params params,
    GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto context = std::make_unique<CallContext>(std::move(callback));
  context_map_.AddWithID(std::move(context), params.call_id());

  auto* runner =
      arc::ArcFileSystemOperationRunner::GetForBrowserContext(profile_);
  if (!runner) {
    // This happens when ARC is not allowed in this profile.
    OnComplete(params.call_id());
    return;
  }

  if (!MatchesFileType(params.file_type())) {
    // Return immediately without results when this root's id does not match the
    // requested file type.
    OnComplete(params.call_id());
    return;
  }

  runner->GetRecentDocuments(
      arc::kMediaDocumentsProviderAuthority, root_id_,
      base::BindOnce(&MediaRoot::OnRunnerDone, weak_ptr_factory_.GetWeakPtr(),
                     params));
}

std::vector<RecentFile> RecentArcMediaSource::MediaRoot::Stop(int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return {};
  }
  // Just remove context from the map; this cancells the callback and erases the
  // document_id_to_file map.
  context_map_.Remove(call_id);

  // TODO(b:244395002) Add code that returns results collected so far.
  return {};
}

void RecentArcMediaSource::MediaRoot::OnRunnerDone(
    const Params& params,
    std::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents) {
  if (!lag_.is_positive()) {
    OnGotRecentDocuments(params, std::move(maybe_documents));
    return;
  }

  if (!timer_) {
    timer_ = std::make_unique<base::OneShotTimer>();
  }
  timer_->Start(FROM_HERE, lag_,
                base::BindOnce(&MediaRoot::OnGotRecentDocuments,
                               weak_ptr_factory_.GetWeakPtr(), params,
                               std::move(maybe_documents)));
}

void RecentArcMediaSource::MediaRoot::OnGotRecentDocuments(
    const Params& params,
    std::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CallContext* context = context_map_.Lookup(params.call_id());
  if (context == nullptr) {
    return;
  }

  // Initialize |document_id_to_file_| with recent document IDs returned.
  if (maybe_documents.has_value()) {
    const std::u16string q16 = base::UTF8ToUTF16(params.query());
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
      context->document_id_to_file.emplace(document->document_id, std::nullopt);
    }
  }

  if (context->document_id_to_file.empty()) {
    OnComplete(params.call_id());
    return;
  }

  // We have several recent documents, so start searching their real paths.
  ScanDirectory(params, base::FilePath());
}

void RecentArcMediaSource::MediaRoot::ScanDirectory(
    const Params& params,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If context was cleared while we were scanning directories, just abandon
  // this effort.
  CallContext* context = context_map_.Lookup(params.call_id());
  if (context == nullptr) {
    return;
  }

  ++context->num_inflight_readdirs;

  auto* root_map =
      arc::ArcDocumentsProviderRootMap::GetForBrowserContext(profile_);
  if (!root_map) {
    // We already checked ARC is allowed for this profile (indirectly), so
    // this should never happen.
    LOG(ERROR) << "ArcDocumentsProviderRootMap is not available";
    OnReadDirectory(params, path, base::File::FILE_ERROR_FAILED, {});
    return;
  }

  auto* root =
      root_map->Lookup(arc::kMediaDocumentsProviderAuthority, root_id_);
  if (!root) {
    // Media roots should always exist.
    LOG(ERROR) << "ArcDocumentsProviderRoot is missing";
    OnReadDirectory(params, path, base::File::FILE_ERROR_NOT_FOUND, {});
    return;
  }

  root->ReadDirectory(
      path, base::BindOnce(&RecentArcMediaSource::MediaRoot::OnReadDirectory,
                           weak_ptr_factory_.GetWeakPtr(), params, path));
}

void RecentArcMediaSource::MediaRoot::OnReadDirectory(
    const Params& params,
    const base::FilePath& path,
    base::File::Error result,
    std::vector<arc::ArcDocumentsProviderRoot::ThinFileInfo> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If callback was cleared while we were scanning directories just abandon
  // this effort.
  CallContext* context = context_map_.Lookup(params.call_id());
  if (context == nullptr) {
    return;
  }

  for (const auto& file : files) {
    base::FilePath subpath = path.Append(file.name);
    if (file.is_directory) {
      if (!params.IsLate()) {
        ScanDirectory(params, subpath);
      }
      continue;
    }

    auto doc_it = context->document_id_to_file.find(file.document_id);
    if (doc_it == context->document_id_to_file.end()) {
      continue;
    }

    // Update |document_id_to_file_|.
    // We keep the lexicographically smallest URL to stabilize the results when
    // there are multiple files with the same document ID.
    auto url = BuildDocumentsProviderUrl(params, subpath);
    std::optional<RecentFile>& entry = doc_it->second;
    if (!entry.has_value() ||
        storage::FileSystemURL::Comparator()(url, entry.value().url())) {
      entry = RecentFile(url, file.last_modified);
    }
  }

  --context->num_inflight_readdirs;
  DCHECK_LE(0, context->num_inflight_readdirs);

  if (context->num_inflight_readdirs == 0) {
    OnComplete(params.call_id());
  }
}

void RecentArcMediaSource::MediaRoot::OnComplete(int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    return;
  }
  DCHECK_EQ(0, context->num_inflight_readdirs);
  DCHECK(!context->callback.is_null());

  std::vector<RecentFile> files;
  for (const auto& entry : context->document_id_to_file) {
    const std::optional<RecentFile>& file = entry.second;
    if (file.has_value()) {
      files.emplace_back(file.value());
    }
  }

  std::move(context->callback).Run(std::move(files));
  context_map_.Remove(call_id);
}

storage::FileSystemURL
RecentArcMediaSource::MediaRoot::BuildDocumentsProviderUrl(
    const Params& params,
    const base::FilePath& path) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  return mount_points->CreateExternalFileSystemURL(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(params.origin())),
      arc::kDocumentsProviderMountPointName, relative_mount_path_.Append(path));
}

bool RecentArcMediaSource::MediaRoot::MatchesFileType(
    FileType file_type) const {
  switch (file_type) {
    case FileType::kAll:
      return true;
    case FileType::kImage:
      return root_id_ == arc::kImagesRootId;
    case FileType::kVideo:
      return root_id_ == arc::kVideosRootId;
    case FileType::kDocument:
      return root_id_ == arc::kDocumentsRootId;
    default:
      return false;
  }
}

RecentArcMediaSource::CallContext::CallContext(GetRecentFilesCallback callback)
    : callback(std::move(callback)), build_start_time(base::TimeTicks::Now()) {}
RecentArcMediaSource::CallContext::CallContext(CallContext&& context)
    : callback(std::move(context.callback)),
      build_start_time(std::move(context.build_start_time)),
      active_roots(std::move(context.active_roots)),
      files(std::move(context.files)) {}

RecentArcMediaSource::CallContext::~CallContext() = default;

const char RecentArcMediaSource::kLoadHistogramName[] =
    "FileBrowser.Recent.LoadArcMedia";

RecentArcMediaSource::RecentArcMediaSource(Profile* profile, size_t max_files)
    : profile_(profile), max_files_(max_files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const char* root_id : kMediaDocumentsProviderRootIds) {
    roots_.emplace(std::make_pair(
        root_id, std::make_unique<MediaRoot>(root_id, profile_)));
  }
}

RecentArcMediaSource::~RecentArcMediaSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RecentArcMediaSource::GetRecentFiles(Params params,
                                          GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_map_.Lookup(params.call_id()) == nullptr);

  // If ARC file system operations will be deferred, return immediately without
  // recording UMA metrics.
  //
  // TODO(nya): Return files progressively rather than simply giving up.
  // Also, it is wrong to assume all following operations will not be deferred
  // just because this function returned true. However, in practice, it is rare
  // ArcFileSystemOperationRunner's deferring state switches from disabled to
  // enabled (one such case is when ARC container crashes).
  if (!WillArcFileSystemOperationsRunImmediately()) {
    std::move(callback).Run({});
    return;
  }

  if (roots_.empty()) {
    OnComplete(params.call_id());
    return;
  }

  auto context = std::make_unique<CallContext>(std::move(callback));

  // Active sources must be set before the loop that calls GetRecentFiles is
  // executed. MediaRoot may call the callback immediately which modifies the
  // active_roots_, resulting in immediate call to OnComplete.
  for (const auto& [_, source] : roots_) {
    context->active_roots.insert(source.get());
  }

  context_map_.AddWithID(std::move(context), params.call_id());

  for (const auto& [_, source] : roots_) {
    source->GetRecentFiles(
        params, base::BindOnce(&RecentArcMediaSource::OnGotRecentFiles,
                               weak_ptr_factory_.GetWeakPtr(), params.call_id(),
                               source.get()));
  }
}

std::vector<RecentFile> RecentArcMediaSource::Stop(int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    // Here we assume that the call to stop came just after we returned the
    // results in the OnComplete method.
    return {};
  }

  // We do not call the callback, so just clean it up.
  context->callback.Reset();
  // Copy the files we collected so far.
  std::vector<RecentFile> files(context->files.begin(), context->files.end());

  // For all roots still active, stop them, get their partial results and append
  // them to the results we have so far.
  for (const auto& root : context->active_roots) {
    base::Extend(files, root->Stop(call_id));
  }
  context_map_.Remove(call_id);
  return PrepareResponse(std::move(files), max_files_);
}

void RecentArcMediaSource::OnGotRecentFiles(
    const int32_t call_id,
    RecentArcMediaSource::MediaRoot* root,
    std::vector<RecentFile> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    // If we cannot find the context that means the Stop method has been called
    // before we got here. Just return.
    return;
  }

  context->active_roots.erase(root);
  base::Extend(context->files, files);

  if (context->active_roots.empty()) {
    OnComplete(call_id);
  }
}

void RecentArcMediaSource::OnComplete(const int32_t call_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CallContext* context = context_map_.Lookup(call_id);
  if (context == nullptr) {
    // If we cannot find the context that means the Stop method has been called.
    // Just return immediately.
    return;
  }

  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - context->build_start_time);

  std::move(context->callback)
      .Run(PrepareResponse(std::move(context->files), max_files_));
  context_map_.Remove(call_id);
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

bool RecentArcMediaSource::SetLagForTesting(const char* media_root,
                                            const base::TimeDelta& lag) {
  const auto& it = roots_.find(media_root);
  if (it == roots_.end()) {
    return false;
  }
  it->second->SetLagForTesting(lag);  // IN-TEST
  return true;
}

}  // namespace ash
