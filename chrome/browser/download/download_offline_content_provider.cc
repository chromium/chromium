// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_offline_content_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnail/generator/image_thumbnail_request.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/download/android/download_controller.h"
#include "chrome/browser/download/android/download_manager_bridge.h"
#include "chrome/browser/download/android/download_manager_service.h"
#include "chrome/browser/download/android/download_utils.h"
#include "chrome/browser/download/android/open_download_dialog_bridge_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/pdf/common/constants.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/common/content_features.h"
#include "ui/base/device_form_factor.h"
#endif

using OfflineItemFilter = offline_items_collection::OfflineItemFilter;
using OfflineItemState = offline_items_collection::OfflineItemState;
using OfflineItemProgressUnit =
    offline_items_collection::OfflineItemProgressUnit;
using offline_items_collection::OfflineItemShareInfo;
using OfflineItemVisuals = offline_items_collection::OfflineItemVisuals;
using UpdateDelta = offline_items_collection::UpdateDelta;

namespace {

// Thumbnail size used for generating thumbnails for image files.
const int kThumbnailSizeInDP = 64;

// The delay to wait after loading history and before starting the check for
// externally removed downloads.
const base::TimeDelta kCheckExternallyRemovedDownloadsDelay =
    base::Milliseconds(100);

#if BUILDFLAG(IS_ANDROID)
// Invalid system download Id.
const int kInvalidSystemDownloadId = -1;

#endif

bool ShouldShowDownloadItem(const DownloadItem* item) {
  return !item->IsTemporary() && !item->IsTransient() && !item->IsDangerous() &&
         !item->GetTargetFilePath().empty();
}

std::unique_ptr<OfflineItemShareInfo> CreateShareInfo(
    const DownloadItem* item) {
  auto share_info = std::make_unique<OfflineItemShareInfo>();
#if BUILDFLAG(IS_ANDROID)
  if (item) {
    share_info->uri =
        DownloadUtils::GetUriStringForPath(item->GetTargetFilePath());
  }
#else
  NOTIMPLEMENTED();
#endif
  return share_info;
}

// Observes the all downloads, primrarily responsible for cleaning up the
// externally removed downloads, and notifying the provider about download
// deletions. Only used for android.
class AllDownloadObserver
    : public download::AllDownloadEventNotifier::Observer {
 public:
  explicit AllDownloadObserver(DownloadOfflineContentProvider* provider);

  AllDownloadObserver(const AllDownloadObserver&) = delete;
  AllDownloadObserver& operator=(const AllDownloadObserver&) = delete;

  ~AllDownloadObserver() override;

  void OnDownloadUpdated(SimpleDownloadManagerCoordinator* manager,
                         DownloadItem* item) override;
  void OnDownloadRemoved(SimpleDownloadManagerCoordinator* manager,
                         DownloadItem* item) override;

 private:
  void DeleteDownloadItem(SimpleDownloadManagerCoordinator* manager,
                          const std::string& guid);

  raw_ptr<DownloadOfflineContentProvider> provider_;
  base::WeakPtrFactory<AllDownloadObserver> weak_ptr_factory_{this};
};

AllDownloadObserver::AllDownloadObserver(
    DownloadOfflineContentProvider* provider)
    : provider_(provider) {}

AllDownloadObserver::~AllDownloadObserver() {}

void AllDownloadObserver::OnDownloadUpdated(
    SimpleDownloadManagerCoordinator* manager,
    DownloadItem* item) {
  if (item->GetFileExternallyRemoved()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AllDownloadObserver::DeleteDownloadItem,
                                  weak_ptr_factory_.GetWeakPtr(), manager,
                                  item->GetGuid()));
  }
}

void AllDownloadObserver::OnDownloadRemoved(
    SimpleDownloadManagerCoordinator* manager,
    DownloadItem* item) {
  provider_->OnDownloadRemoved(item);
}

void AllDownloadObserver::DeleteDownloadItem(
    SimpleDownloadManagerCoordinator* manager,
    const std::string& guid) {
  DownloadItem* item = manager->GetDownloadByGuid(guid);
  if (item)
    item->Remove();
}

}  // namespace

DownloadOfflineContentProvider::DownloadOfflineContentProvider(
    OfflineContentAggregator* aggregator,
    const std::string& name_space)
    : aggregator_(aggregator),
      name_space_(name_space),
      manager_(nullptr),
      checked_for_externally_removed_downloads_(false),
      state_(State::UNINITIALIZED),
      profile_(nullptr) {
  aggregator_->RegisterProvider(name_space_, this);
#if BUILDFLAG(IS_ANDROID)
  all_download_observer_ = std::make_unique<AllDownloadObserver>(this);
#endif
}

DownloadOfflineContentProvider::~DownloadOfflineContentProvider() {
  aggregator_->UnregisterProvider(name_space_);
  if (manager_) {
    manager_->RemoveObserver(this);
    if (all_download_observer_)
      manager_->GetNotifier()->RemoveObserver(all_download_observer_.get());
  }
}

void DownloadOfflineContentProvider::SetSimpleDownloadManagerCoordinator(
    SimpleDownloadManagerCoordinator* manager) {
  DCHECK(manager);
  if (manager_ == manager)
    return;

  manager_ = manager;
  manager_->AddObserver(this);

  if (all_download_observer_)
    manager_->GetNotifier()->AddObserver(all_download_observer_.get());
}

void DownloadOfflineContentProvider::OnDownloadsInitialized(
    bool active_downloads_only) {
  state_ = active_downloads_only ? State::ACTIVE_DOWNLOADS_ONLY
                                 : State::HISTORY_LOADED;

  while (!pending_actions_for_reduced_mode_.empty()) {
    auto callback = std::move(pending_actions_for_reduced_mode_.front());
    pending_actions_for_reduced_mode_.pop_front();
    std::move(callback).Run();
  }

  if (state_ != State::HISTORY_LOADED)
    return;

  while (!pending_actions_for_full_browser_.empty()) {
    auto callback = std::move(pending_actions_for_full_browser_.front());
    pending_actions_for_full_browser_.pop_front();
    std::move(callback).Run();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &DownloadOfflineContentProvider::CheckForExternallyRemovedDownloads,
          weak_ptr_factory_.GetWeakPtr()),
      kCheckExternallyRemovedDownloadsDelay);
}

// TODO(shaktisahu) : Pass DownloadOpenSource.
void DownloadOfflineContentProvider::OpenItem(const OpenParams& open_params,
                                              const ContentId& id) {
  EnsureDownloadCoreServiceStarted();
  if (state_ != State::HISTORY_LOADED) {
    pending_actions_for_full_browser_.push_back(
        base::BindOnce(&DownloadOfflineContentProvider::OpenItem,
                       weak_ptr_factory_.GetWeakPtr(), open_params, id));
    return;
  }

  DownloadItem* item = GetDownload(id.id);
  if (item)
    item->OpenDownload();
}

void DownloadOfflineContentProvider::RemoveItem(const ContentId& id) {
  EnsureDownloadCoreServiceStarted();
  if (state_ != State::HISTORY_LOADED) {
    pending_actions_for_full_browser_.push_back(
        base::BindOnce(&DownloadOfflineContentProvider::RemoveItem,
                       weak_ptr_factory_.GetWeakPtr(), id));
    return;
  }

  DownloadItem* item = GetDownload(id.id);
  if (item) {
    item->DeleteFile(base::DoNothing());
    item->Remove();
  }
}

void DownloadOfflineContentProvider::CancelDownload(const ContentId& id) {
  if (state_ == State::UNINITIALIZED) {
    pending_actions_for_reduced_mode_.push_back(
        base::BindOnce(&DownloadOfflineContentProvider::CancelDownload,
                       weak_ptr_factory_.GetWeakPtr(), id));
    return;
  }

  DownloadItem* item = GetDownload(id.id);
  if (item)
    item->Cancel(true);
}

void DownloadOfflineContentProvider::PauseDownload(const ContentId& id) {
  if (state_ == State::UNINITIALIZED) {
    pending_actions_for_reduced_mode_.push_back(
        base::BindOnce(&DownloadOfflineContentProvider::PauseDownload,
                       weak_ptr_factory_.GetWeakPtr(), id));
    return;
  }

  DownloadItem* item = GetDownload(id.id);
  if (item)
    item->Pause();
}

void DownloadOfflineContentProvider::ResumeDownload(const ContentId& id) {
  if (state_ == State::UNINITIALIZED) {
    pending_actions_for_reduced_mode_.push_back(
        base::BindOnce(&DownloadOfflineContentProvider::ResumeDownload,
                       weak_ptr_factory_.GetWeakPtr(), id));
    return;
  }

  DownloadItem* item = GetDownload(id.id);
  if (item)
    item->Resume(true /* user_resume */);
}

void DownloadOfflineContentProvider::GetItemById(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback) {
  EnsureDownloadCoreServiceStarted();
  base::OnceClosure run_get_item_callback =
      base::BindOnce(&DownloadOfflineContentProvider::RunGetItemByIdCallback,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback));
  if (state_ != State::HISTORY_LOADED) {
    pending_actions_for_full_browser_.push_back(
        std::move(run_get_item_callback));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(run_get_item_callback));
}

void DownloadOfflineContentProvider::GetAllItems(
    OfflineContentProvider::MultipleItemCallback callback) {
  EnsureDownloadCoreServiceStarted();
  base::OnceClosure run_get_all_items_callback =
      base::BindOnce(&DownloadOfflineContentProvider::RunGetAllItemsCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  if (state_ != State::HISTORY_LOADED) {
    pending_actions_for_full_browser_.push_back(
        std::move(run_get_all_items_callback));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(run_get_all_items_callback));
}

void DownloadOfflineContentProvider::GetVisualsForItem(
    const ContentId& id,
    GetVisualsOptions options,
    VisualsCallback callback) {
  // TODO(crbug.com/40581903) Supply thumbnail if item is visible.
  DownloadItem* item = GetDownload(id.id);
  display::Screen* screen = display::Screen::GetScreen();
  if (!item || !options.get_icon || !screen) {
    // No favicon is available; run the callback without visuals.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, nullptr));
    return;
  }

  display::Display display = screen->GetPrimaryDisplay();
  int icon_size = kThumbnailSizeInDP * display.device_scale_factor();

  auto request = std::make_unique<ImageThumbnailRequest>(
      icon_size,
      base::BindOnce(&DownloadOfflineContentProvider::OnThumbnailRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
  request->Start(item->GetTargetFilePath());

  // Dropping ownership of |request| here because it will clean itself up once
  // the started request finishes.
  request.release();
}

void DownloadOfflineContentProvider::GetShareInfoForItem(
    const ContentId& id,
    ShareCallback callback) {
  EnsureDownloadCoreServiceStarted();
  if (state_ != State::HISTORY_LOADED) {
    pending_actions_for_full_browser_.push_back(base::BindOnce(
        &DownloadOfflineContentProvider::GetShareInfoForItem,
        weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
    return;
  }

  DownloadItem* item = GetDownload(id.id);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), id, CreateShareInfo(item)));
}

void DownloadOfflineContentProvider::OnThumbnailRetrieved(
    const ContentId& id,
    VisualsCallback callback,
    const SkBitmap& bitmap) {
  auto visuals = std::make_unique<OfflineItemVisuals>();
  visuals->icon = gfx::Image::CreateFrom1xBitmap(bitmap);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), id, std::move(visuals)));
}

void DownloadOfflineContentProvider::RenameItem(const ContentId& id,
                                                const std::string& name,
                                                RenameCallback callback) {
  EnsureDownloadCoreServiceStarted();
  if (state_ != State::HISTORY_LOADED) {
    pending_actions_for_full_browser_.push_back(base::BindOnce(
        &DownloadOfflineContentProvider::RenameItem,
        weak_ptr_factory_.GetWeakPtr(), id, name, std::move(callback)));
    return;
  }

  DownloadItem* item = GetDownload(id.id);
  if (!item) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), RenameResult::FAILURE_UNAVAILABLE));
    return;
  }
  download::DownloadItem::RenameDownloadCallback download_callback =
      base::BindOnce(
          &DownloadOfflineContentProvider::OnRenameDownloadCallbackDone,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), item);
  base::FilePath::StringType filename;
#if BUILDFLAG(IS_WIN)
  filename = base::UTF8ToWide(name);
#else
  filename = name;
#endif
  item->Rename(base::FilePath(filename), std::move(download_callback));
}

void DownloadOfflineContentProvider::OnRenameDownloadCallbackDone(
    RenameCallback callback,
    DownloadItem* item,
    DownloadItem::DownloadRenameResult result) {
  if (result == DownloadItem::DownloadRenameResult::SUCCESS) {
    auto offline_item = OfflineItemUtils::CreateOfflineItem(name_space_, item);
    UpdateDelta update_delta;
    update_delta.state_changed = false;
    update_delta.visuals_changed = false;
    UpdateObservers(offline_item, update_delta);
  }

  std::move(callback).Run(
      OfflineItemUtils::ConvertDownloadRenameResultToRenameResult(result));
}

void DownloadOfflineContentProvider::OnManagerGoingDown(
    SimpleDownloadManagerCoordinator* manager) {
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> all_items;
  GetAllDownloads(&all_items);

  for (download::DownloadItem* item : all_items) {
    if (!ShouldShowDownloadItem(item))
      continue;
    NotifyItemRemoved(ContentId(name_space_, item->GetGuid()));
  }

  manager_ = nullptr;
}

void DownloadOfflineContentProvider::OnDownloadStarted(DownloadItem* item) {
  item->RemoveObserver(this);
  item->AddObserver(this);

  OnDownloadUpdated(item);
}

void DownloadOfflineContentProvider::OnDownloadUpdated(DownloadItem* item) {
  // Notify user if this download is blocked.
  bool should_notify =
      item->GetState() == DownloadItem::INTERRUPTED &&
      item->GetLastReason() ==
          download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED &&
      item->GetInsecureDownloadStatus() !=
          download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK;
  // Wait until the target path is determined or the download is canceled.
  if (!should_notify && item->GetTargetFilePath().empty() &&
      item->GetState() != DownloadItem::CANCELLED) {
    return;
  }

  if (!should_notify && !ShouldShowDownloadItem(item)) {
    return;
  }

  UpdateDelta update_delta;
  auto offline_item = OfflineItemUtils::CreateOfflineItem(name_space_, item);
  if (offline_item.state == OfflineItemState::COMPLETE ||
      offline_item.state == OfflineItemState::FAILED ||
      offline_item.state == OfflineItemState::CANCELLED) {
    // TODO(crbug.com/40616574): May be move this to DownloadItem.
    // Never call this for completed downloads from history.
    item->RemoveObserver(this);

    update_delta.state_changed = true;
    if (item->GetState() == DownloadItem::COMPLETE)
      AddCompletedDownload(item);
  }

  UpdateObservers(offline_item, update_delta);
}

void DownloadOfflineContentProvider::OnDownloadRemoved(DownloadItem* item) {
  if (!ShouldShowDownloadItem(item))
    return;

#if BUILDFLAG(IS_ANDROID)
  DownloadManagerBridge::RemoveCompletedDownload(item);
#endif

  ContentId contentId(name_space_, item->GetGuid());
  NotifyItemRemoved(contentId);
}

void DownloadOfflineContentProvider::OnProfileCreated(Profile* profile) {
  profile_ = profile;
}

void DownloadOfflineContentProvider::AddCompletedDownload(DownloadItem* item) {
#if BUILDFLAG(IS_ANDROID)
  base::OnceCallback<void(int64_t)> cb =
      base::BindOnce(&DownloadOfflineContentProvider::AddCompletedDownloadDone,
                     weak_ptr_factory_.GetWeakPtr(), item->GetGuid());
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_Q) {
    DownloadManagerBridge::AddCompletedDownload(item, std::move(cb));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), kInvalidSystemDownloadId));
  }
#endif
}

void DownloadOfflineContentProvider::AddCompletedDownloadDone(
    const std::string& download_guid,
    int64_t system_download_id) {
#if BUILDFLAG(IS_ANDROID)
  DownloadItem* item = GetDownload(download_guid);
  if (!item)
    return;
  if (DownloadUtils::IsOmaDownloadDescription(item->GetMimeType())) {
    DownloadManagerService::GetInstance()->HandleOMADownload(
        item, system_download_id);
    return;
  }

  if (profile_ && profile_->GetDownloadManagerDelegate() &&
      profile_->GetDownloadManagerDelegate()->ShouldOpenPdfInline() &&
      item->GetMimeType() == pdf::kPDFMimeType) {
    return;
  }

  if (DownloadUtils::ShouldAutoOpenDownload(item)) {
    item->OpenDownload();
  } else if (item->IsFromExternalApp()) {
    if (item->GetMimeType() == pdf::kPDFMimeType) {
      if (profile_ &&
          DownloadPrefs::FromBrowserContext(profile_)->IsAutoOpenPdfEnabled()) {
        item->OpenDownload();
      } else {
        open_download_dialog_delegate_.CreateDialog(item);
      }
    }
  }
#endif
}

DownloadItem* DownloadOfflineContentProvider::GetDownload(
    const std::string& download_guid) {
  return manager_ ? manager_->GetDownloadByGuid(download_guid) : nullptr;
}

void DownloadOfflineContentProvider::GetAllDownloads(
    std::vector<raw_ptr<DownloadItem, VectorExperimental>>* all_items) {
  if (manager_)
    manager_->GetAllDownloads(all_items);
}

void DownloadOfflineContentProvider::UpdateObservers(
    const OfflineItem& item,
    const std::optional<UpdateDelta>& update_delta) {
  NotifyItemUpdated(item, update_delta);
}

void DownloadOfflineContentProvider::CheckForExternallyRemovedDownloads() {
  if (checked_for_externally_removed_downloads_ || !manager_)
    return;

  checked_for_externally_removed_downloads_ = true;

#if BUILDFLAG(IS_ANDROID)
  manager_->CheckForExternallyRemovedDownloads();
#endif
}

void DownloadOfflineContentProvider::EnsureDownloadCoreServiceStarted() {
  DCHECK(profile_);
  CHECK(profile_->GetDownloadManager());
}

void DownloadOfflineContentProvider::RunGetAllItemsCallback(
    OfflineContentProvider::MultipleItemCallback callback) {
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> all_items;
  GetAllDownloads(&all_items);

  std::vector<OfflineItem> items;
  for (download::DownloadItem* item : all_items) {
    if (!ShouldShowDownloadItem(item))
      continue;
    items.push_back(OfflineItemUtils::CreateOfflineItem(name_space_, item));
  }
  std::move(callback).Run(std::move(items));
}

void DownloadOfflineContentProvider::RunGetItemByIdCallback(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback) {
  DownloadItem* item = GetDownload(id.id);
  auto offline_item =
      item && ShouldShowDownloadItem(item)
          ? std::make_optional(
                OfflineItemUtils::CreateOfflineItem(name_space_, item))
          : std::nullopt;

  std::move(callback).Run(offline_item);
}
