// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/download_metadata_manager.h"

#include <limits.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"

namespace safe_browsing {

namespace {

// The name of the metadata file in the profile directory.
const base::FilePath::CharType kDownloadMetadataBasename[] =
    FILE_PATH_LITERAL("DownloadMetadata");


// DownloadItemData ------------------------------------------------------------

// A UserData object that holds the ClientDownloadRequest for a download while
// it is in progress.
class DownloadItemData : public base::SupportsUserData::Data {
 public:
  DownloadItemData(const DownloadItemData&) = delete;
  DownloadItemData& operator=(const DownloadItemData&) = delete;

  ~DownloadItemData() override {}

  // Sets the ClientDownloadRequest for a given DownloadItem.
  static void SetRequestForDownload(
      download::DownloadItem* item,
      std::unique_ptr<ClientDownloadRequest> request);

  // Returns the ClientDownloadRequest for a download or null if there is none.
  static std::unique_ptr<ClientDownloadRequest> TakeRequestForDownload(
      download::DownloadItem* item);

 private:
  // A unique id for associating metadata with a download::DownloadItem.
  static const void* const kKey_;

  explicit DownloadItemData(std::unique_ptr<ClientDownloadRequest> request)
      : request_(std::move(request)) {}

  std::unique_ptr<ClientDownloadRequest> request_;
};

// Make the key's value unique by setting it to its own location.
// static
const void* const DownloadItemData::kKey_ = &DownloadItemData::kKey_;

// static
void DownloadItemData::SetRequestForDownload(
    download::DownloadItem* item,
    std::unique_ptr<ClientDownloadRequest> request) {
  item->SetUserData(&kKey_,
                    base::WrapUnique(new DownloadItemData(std::move(request))));
}

// static
std::unique_ptr<ClientDownloadRequest> DownloadItemData::TakeRequestForDownload(
    download::DownloadItem* item) {
  DownloadItemData* data =
      static_cast<DownloadItemData*>(item->GetUserData(&kKey_));
  if (!data)
    return nullptr;
  std::unique_ptr<ClientDownloadRequest> request = std::move(data->request_);
  item->RemoveUserData(&kKey_);
  return request;
}


// Utility functions------------------------------------------------------------

// Returns the path to the metadata file for |browser_context|.
base::FilePath GetMetadataPath(content::BrowserContext* browser_context) {
  return browser_context->GetPath().Append(kDownloadMetadataBasename);
}

// Returns true if |metadata| appears to be valid.
bool MetadataIsValid(const DownloadMetadata& metadata) {
  return (metadata.has_download_id() &&
          metadata.has_download() &&
          metadata.download().has_download() &&
          metadata.download().download().has_url());
}

// Reads and parses a DownloadMetadata message from |metadata_path| into
// |metadata|.
void ReadMetadataInBackground(const base::FilePath& metadata_path,
                              DownloadMetadata* metadata) {
  using base::File;
  DCHECK(metadata);
  File metadata_file(metadata_path, File::FLAG_OPEN | File::FLAG_READ);
  base::File::Info info;
  if (metadata_file.IsValid() && metadata_file.GetInfo(&info) &&
      info.size <= INT_MAX) {
    auto file_data = base::HeapArray<uint8_t>::Uninit(info.size);
    if (metadata_file.Read(0, file_data).value_or(0) &&
        metadata->ParseFromArray(file_data.data(),
                                 static_cast<int>(file_data.size())) &&
        MetadataIsValid(*metadata)) {
      return;
    }
  }
  metadata->Clear();
}

// Writes |download_metadata| to |metadata_path|.
void WriteMetadataInBackground(const base::FilePath& metadata_path,
                               DownloadMetadata* download_metadata) {
  std::string file_data;
  if (download_metadata->SerializeToString(&file_data)) {
    base::ImportantFileWriter::WriteFileAtomically(metadata_path, file_data);
  }
}

// Deletes |metadata_path|.
void DeleteMetadataInBackground(const base::FilePath& metadata_path) {
  base::DeleteFile(metadata_path);
}

// Runs |callback| with the DownloadDetails in |download_metadata|.
void ReturnResults(DownloadMetadataManager::GetDownloadDetailsCallback callback,
                   std::unique_ptr<DownloadMetadata> download_metadata) {
  if (!download_metadata->has_download_id())
    std::move(callback).Run(
        std::unique_ptr<ClientIncidentReport_DownloadDetails>());
  else
    std::move(callback).Run(
        base::WrapUnique(download_metadata->release_download()));
}

}  // namespace

// Applies operations to the profile's persistent DownloadMetadata as they occur
// on its corresponding download item. An instance can be in one of three
// states: waiting for metatada load, waiting for metadata to load after its
// corresponding SimpleDownloadManagerCoordinator has gone down, and not waiting
// for metadata to load. The instance is notified on events for downloads
// belonging to its coordinator. While it is waiting for metadata to load, it
// records all operations on download items that must be reflected in the
// metadata. Once the metadata is ready, recorded operations are applied to the
// metadata. While waiting for metadata to load, an instance also tracks
// callbacks to be run to provide consumers with persisted details of a
// download.
class DownloadMetadataManager::ManagerContext {
 public:
  ManagerContext(scoped_refptr<base::SequencedTaskRunner> task_runner,
                 content::BrowserContext& browser_context);

  ManagerContext(const ManagerContext&) = delete;
  ManagerContext& operator=(const ManagerContext&) = delete;

  content::BrowserContext* browser_context() { return &browser_context_.get(); }

  // Detaches this context from its owner. The owner must not access the context
  // following this call. The context will be deleted immediately if it is not
  // waiting for a metadata load with either recorded operations or pending
  // callbacks.
  void Detach();

  // Sets |request| as the relevant metadata to persist for |download| if or
  // when it is complete. If |request| is null, the metadata for |download|
  // is/will be removed.
  void SetRequest(download::DownloadItem* download,
                  std::unique_ptr<ClientDownloadRequest> request);

  // Gets the persisted DownloadDetails. |callback| will be run immediately if
  // the data is available. Otherwise, it will be run later on the caller's
  // thread.
  void GetDownloadDetails(GetDownloadDetailsCallback callback);

  void OnDownloadUpdated(download::DownloadItem* download);
  void OnDownloadOpened(download::DownloadItem* download);
  void OnDownloadRemoved(download::DownloadItem* download);

 private:
  enum State {
    // The context is waiting for the metadata file to be loaded.
    WAITING_FOR_LOAD,

    // The context is waiting for the metadata file to be loaded and its
    // corresponding SimpleDownloadManagerCoordinator has gone away.
    DETACHED_WAIT,

    // The context has loaded the metadata file.
    LOAD_COMPLETE,
  };

  struct ItemData {
    ItemData() : removed() {}
    base::Time last_opened_time;
    bool removed;
  };

  // A mapping of download IDs to their corresponding data.
  typedef std::map<uint32_t, ItemData> ItemDataMap;

  ~ManagerContext();

  // Commits |request| to the DownloadDetails for |item|'s BrowserContext.
  // Callbacks will be run immediately if the context had been waiting for a
  // load (which will be abandoned).
  void CommitRequest(download::DownloadItem* item,
                     std::unique_ptr<ClientDownloadRequest> request);

  // Posts a background task to read the metadata from disk.
  void ReadMetadata();

  // Posts a background task to write the metadata to disk.
  void WriteMetadata();

  // Removes metadata for the context from memory and posts a background task to
  // delete it on disk.
  void RemoveMetadata();

  // Clears the |pending_items_| mapping.
  void ClearPendingItems();

  // Runs all |get_details_callbacks_| with the current metadata.
  void RunCallbacks();

  // Returns true if metadata corresponding to |item| is available.
  bool HasMetadataFor(const download::DownloadItem* item) const;

  // A callback run on the main thread with the results from reading the
  // metadata file from disk.
  void OnMetadataReady(std::unique_ptr<DownloadMetadata> download_metadata);

  // Updates the last opened time in the metadata and writes it to disk.
  void UpdateLastOpenedTime(const base::Time& last_opened_time);

  // A task runner to which IO tasks are posted.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  const raw_ref<content::BrowserContext> browser_context_;

  // The path to the metadata file for this context.
  base::FilePath metadata_path_;

  // When not LOAD_COMPLETE, the context is waiting for a pending read operation
  // to complete. While this is the case, events are temporarily recorded in
  // |pending_items_|. Once the read completes, pending operations for the item
  // corresponding to the metadata file are applied to the file and all other
  // recorded data are dropped. Queued GetDownloadDetailsCallbacks are run upon
  // read completion as well. The context is moved to the DETACHED_WAIT state if
  // the corresponding SimpleDownloadManagerCoordinator goes away while a read
  // operation is outstanding. When the read subsequently completes, the context
  // is destroyed after the processing described above is performed.
  State state_;

  // The current metadata for the context. May be supplied either by reading
  // from the file or by having been set via |SetRequest|.
  std::unique_ptr<DownloadMetadata> download_metadata_;

  // The operation data that accumulates for added download items while the
  // metadata file is being read.
  ItemDataMap pending_items_;

  // Pending callbacks in response to GetDownloadDetails. The callbacks are run
  // in order when a pending read operation completes.
  std::list<GetDownloadDetailsCallback> get_details_callbacks_;

  base::WeakPtrFactory<ManagerContext> weak_factory_{this};
};

// DownloadMetadataManager -----------------------------------------------------

DownloadMetadataManager::DownloadMetadataManager()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::MayBlock()})) {}

DownloadMetadataManager::~DownloadMetadataManager() {
  // Destruction may have taken place before coordinators have gone down.
  for (auto [coordinator, context] : contexts_) {
    coordinator->GetNotifier()->RemoveObserver(this);
    context->Detach();
  }
  contexts_.clear();
}

void DownloadMetadataManager::AddDownloadManager(
    content::DownloadManager* download_manager) {
  content::BrowserContext* const browser_context =
      download_manager->GetBrowserContext();
  download::SimpleDownloadManagerCoordinator* const coordinator =
      GetCoordinatorForBrowserContext(browser_context);

  // Nothing to do if this coordinator is already being observed.
  if (base::Contains(contexts_, coordinator)) {
    return;
  }

  coordinator->GetNotifier()->AddObserver(this);
  contexts_[coordinator] = new ManagerContext(task_runner_, *browser_context);
}

void DownloadMetadataManager::SetRequest(download::DownloadItem* item,
                                         const ClientDownloadRequest* request) {
  DCHECK(request);
  download::SimpleDownloadManagerCoordinator* const coordinator =
      GetCoordinatorForBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(item));
  DCHECK(base::Contains(contexts_, coordinator));
  contexts_[coordinator]->SetRequest(
      item, std::make_unique<ClientDownloadRequest>(*request));
}

void DownloadMetadataManager::GetDownloadDetails(
    content::BrowserContext* browser_context,
    GetDownloadDetailsCallback callback) {
  DCHECK(browser_context);
  // The coordinator for |browser_context| may not have been created yet. In
  // this case, asking for it would cause history to load in the background and
  // wouldn't really help much. Instead, scan the contexts to see if one belongs
  // to |browser_context|. If one is not found, read the metadata and return it.
  std::unique_ptr<ClientIncidentReport_DownloadDetails> download_details;
  for (auto [coordinator, context] : contexts_) {
    if (context->browser_context() == browser_context) {
      context->GetDownloadDetails(std::move(callback));
      return;
    }
  }

  // Fire off a task to load the details and return them to the caller.
  DownloadMetadata* metadata = new DownloadMetadata();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ReadMetadataInBackground,
                     GetMetadataPath(browser_context), metadata),
      base::BindOnce(&ReturnResults, std::move(callback),
                     base::WrapUnique(metadata)));
}

void DownloadMetadataManager::OnManagerGoingDown(
    download::SimpleDownloadManagerCoordinator* coordinator) {
  coordinator->GetNotifier()->RemoveObserver(this);

  auto iter = contexts_.find(coordinator);
  CHECK(iter != contexts_.end());
  ManagerContext* const context = iter->second;
  contexts_.erase(iter);

  context->Detach();
}

void DownloadMetadataManager::OnDownloadUpdated(
    download::SimpleDownloadManagerCoordinator* coordinator,
    download::DownloadItem* download) {
  auto iter = contexts_.find(coordinator);
  CHECK(iter != contexts_.end());
  iter->second->OnDownloadUpdated(download);
}

void DownloadMetadataManager::OnDownloadOpened(
    download::SimpleDownloadManagerCoordinator* coordinator,
    download::DownloadItem* download) {
  auto iter = contexts_.find(coordinator);
  CHECK(iter != contexts_.end());
  iter->second->OnDownloadOpened(download);
}

void DownloadMetadataManager::OnDownloadRemoved(
    download::SimpleDownloadManagerCoordinator* coordinator,
    download::DownloadItem* download) {
  auto iter = contexts_.find(coordinator);
  CHECK(iter != contexts_.end());
  iter->second->OnDownloadRemoved(download);
}

download::SimpleDownloadManagerCoordinator*
DownloadMetadataManager::GetCoordinatorForBrowserContext(
    content::BrowserContext* context) {
  return SimpleDownloadManagerCoordinatorFactory::GetForKey(
      Profile::FromBrowserContext(context)->GetProfileKey());
}

// DownloadMetadataManager::ManagerContext -------------------------------------

DownloadMetadataManager::ManagerContext::ManagerContext(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    content::BrowserContext& browser_context)
    : task_runner_(std::move(task_runner)),
      browser_context_(browser_context),
      metadata_path_(GetMetadataPath(&browser_context)),
      state_(WAITING_FOR_LOAD) {
  // Start the asynchronous task to read the persistent metadata.
  ReadMetadata();
}

void DownloadMetadataManager::ManagerContext::Detach() {
  // Delete the instance immediately if there's no work to process after a
  // pending read completes.
  if (get_details_callbacks_.empty() && pending_items_.empty()) {
    delete this;
  } else {
    // delete the instance in OnMetadataReady.
    state_ = DETACHED_WAIT;
  }
}

void DownloadMetadataManager::ManagerContext::SetRequest(
    download::DownloadItem* download,
    std::unique_ptr<ClientDownloadRequest> request) {
  DCHECK(request);
  // Hold on to the request for completion time if the download is in progress.
  // Otherwise, commit the request.
  if (download->GetState() == download::DownloadItem::IN_PROGRESS)
    DownloadItemData::SetRequestForDownload(download, std::move(request));
  else
    CommitRequest(download, std::move(request));
}

void DownloadMetadataManager::ManagerContext::GetDownloadDetails(
    GetDownloadDetailsCallback callback) {
  if (state_ != LOAD_COMPLETE) {
    get_details_callbacks_.push_back(std::move(callback));
  } else {
    std::move(callback).Run(
        download_metadata_
            ? std::make_unique<ClientIncidentReport_DownloadDetails>(
                  download_metadata_->download())
            : nullptr);
  }
}

void DownloadMetadataManager::ManagerContext::OnDownloadUpdated(
    download::DownloadItem* download) {
  // Persist metadata for this download if it has just completed.
  if (download->GetState() == download::DownloadItem::COMPLETE) {
    // Ignore downloads we don't have a ClientDownloadRequest for.
    std::unique_ptr<ClientDownloadRequest> request =
        DownloadItemData::TakeRequestForDownload(download);
    if (request)
      CommitRequest(download, std::move(request));
  }
}

void DownloadMetadataManager::ManagerContext::OnDownloadOpened(
    download::DownloadItem* download) {
  const base::Time now = base::Time::Now();
  if (state_ != LOAD_COMPLETE)
    pending_items_[download->GetId()].last_opened_time = now;
  else if (HasMetadataFor(download))
    UpdateLastOpenedTime(now);
}

void DownloadMetadataManager::ManagerContext::OnDownloadRemoved(
    download::DownloadItem* download) {
  if (state_ != LOAD_COMPLETE)
    pending_items_[download->GetId()].removed = true;
  else if (HasMetadataFor(download))
    RemoveMetadata();
}

DownloadMetadataManager::ManagerContext::~ManagerContext() {
  // A context should not be deleted while waiting for a load to complete.
  DCHECK(pending_items_.empty());
  DCHECK(get_details_callbacks_.empty());
}

void DownloadMetadataManager::ManagerContext::CommitRequest(
    download::DownloadItem* item,
    std::unique_ptr<ClientDownloadRequest> request) {
  DCHECK_EQ(download::DownloadItem::COMPLETE, item->GetState());
  if (state_ != LOAD_COMPLETE) {
    // Abandon the read task since |item| is the new top dog.
    weak_factory_.InvalidateWeakPtrs();
    state_ = LOAD_COMPLETE;
    // Drop any recorded operations.
    ClearPendingItems();
  }
  // Take the request.
  download_metadata_ = std::make_unique<DownloadMetadata>();
  download_metadata_->set_download_id(item->GetId());
  download_metadata_->mutable_download()->set_allocated_download(
      request.release());
  download_metadata_->mutable_download()->set_download_time_msec(
      item->GetEndTime().InMillisecondsSinceUnixEpoch());
  // Persist it.
  WriteMetadata();
  // Run callbacks (only present in case of a transition to LOAD_COMPLETE).
  RunCallbacks();
}

void DownloadMetadataManager::ManagerContext::ReadMetadata() {
  DCHECK_NE(state_, LOAD_COMPLETE);

  DownloadMetadata* metadata = new DownloadMetadata();
  // Do not block shutdown on this read since nothing will come of it.
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ReadMetadataInBackground, metadata_path_, metadata),
      base::BindOnce(&DownloadMetadataManager::ManagerContext::OnMetadataReady,
                     weak_factory_.GetWeakPtr(), base::WrapUnique(metadata)));
}

void DownloadMetadataManager::ManagerContext::WriteMetadata() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteMetadataInBackground, metadata_path_,
                     base::Owned(new DownloadMetadata(*download_metadata_))));
}

void DownloadMetadataManager::ManagerContext::RemoveMetadata() {
  if (state_ != LOAD_COMPLETE) {
    // Abandon the read task since the file is to be removed.
    weak_factory_.InvalidateWeakPtrs();
    state_ = LOAD_COMPLETE;
    // Drop any recorded operations.
    ClearPendingItems();
  }
  // Remove any metadata.
  download_metadata_.reset();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeleteMetadataInBackground, metadata_path_));
  // Run callbacks (only present in case of a transition to LOAD_COMPLETE).
  RunCallbacks();
}

void DownloadMetadataManager::ManagerContext::ClearPendingItems() {
  pending_items_.clear();
}

void DownloadMetadataManager::ManagerContext::RunCallbacks() {
  while (!get_details_callbacks_.empty()) {
    std::move(get_details_callbacks_.front())
        .Run(download_metadata_
                 ? std::make_unique<ClientIncidentReport_DownloadDetails>(
                       download_metadata_->download())
                 : nullptr);
    get_details_callbacks_.pop_front();
  }
}

bool DownloadMetadataManager::ManagerContext::HasMetadataFor(
    const download::DownloadItem* item) const {
  // There must not be metadata if the load is not complete.
  DCHECK(state_ == LOAD_COMPLETE || !download_metadata_);
  return (download_metadata_ &&
          download_metadata_->download_id() == item->GetId());
}

void DownloadMetadataManager::ManagerContext::OnMetadataReady(
    std::unique_ptr<DownloadMetadata> download_metadata) {
  DCHECK_NE(state_, LOAD_COMPLETE);

  const bool is_detached = (state_ == DETACHED_WAIT);

  // Note that any available data has been read.
  state_ = LOAD_COMPLETE;
  if (download_metadata->has_download_id())
    download_metadata_ = std::move(download_metadata);
  else
    download_metadata_.reset();

  // Process all operations that had been held while waiting for the metadata.
  if (download_metadata_) {
    const auto& iter = pending_items_.find(download_metadata_->download_id());
    if (iter != pending_items_.end()) {
      const ItemData& item_data = iter->second;
      if (item_data.removed)
        RemoveMetadata();
      else if (!item_data.last_opened_time.is_null())
        UpdateLastOpenedTime(item_data.last_opened_time);
    }
  }

  // Drop the recorded operations.
  ClearPendingItems();

  // Run callbacks.
  RunCallbacks();

  // Delete the context now if it has been detached.
  if (is_detached)
    delete this;
}

void DownloadMetadataManager::ManagerContext::UpdateLastOpenedTime(
    const base::Time& last_opened_time) {
  download_metadata_->mutable_download()->set_open_time_msec(
      last_opened_time.InMillisecondsSinceUnixEpoch());
  WriteMetadata();
}

}  // namespace safe_browsing
