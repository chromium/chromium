// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/background_fetch/job_details.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/common/download_features.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/browser_thread.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/origin.h"

namespace {
constexpr char kBackgroundFetchNamespacePrefix[] = "background_fetch";
}  // namespace

BackgroundFetchDelegateImpl::BackgroundFetchDelegateImpl(Profile* profile)
    : background_fetch::BackgroundFetchDelegateBase(profile),
      profile_(profile),
      provider_namespace_(
          offline_items_collection::OfflineContentAggregator::
              CreateUniqueNameSpace(kBackgroundFetchNamespacePrefix,
                                    profile->IsOffTheRecord())),
      offline_content_aggregator_(OfflineContentAggregatorFactory::GetForKey(
          profile->GetProfileKey())) {
  DCHECK(profile_);
  DCHECK(!provider_namespace_.empty());
  offline_content_aggregator_->RegisterProvider(provider_namespace_, this);

  // Ensure that downloads UI components are initialized to handle the UI
  // updates.
  if (!base::FeatureList::IsEnabled(
          download::features::
              kUseInProgressDownloadManagerForDownloadService)) {
    profile_->GetDownloadManager();
  }
}

BackgroundFetchDelegateImpl::~BackgroundFetchDelegateImpl() {
  offline_content_aggregator_->UnregisterProvider(provider_namespace_);
}

void BackgroundFetchDelegateImpl::MarkJobComplete(const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  background_fetch::JobDetails* job_details = GetJobDetails(job_id);
  RecordBackgroundFetchDeletingRegistrationUkmEvent(
      job_details->fetch_description->origin, job_details->cancelled_from_ui);

  BackgroundFetchDelegateBase::MarkJobComplete(job_id);
}

void BackgroundFetchDelegateImpl::UpdateUI(
    const std::string& job_id,
    const std::optional<std::string>& title,
    const std::optional<SkBitmap>& icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(title || icon);             // One of the UI options must be updatable.
  DCHECK(!icon || !icon->isNull());  // The |icon|, if provided, is not null.

  background_fetch::JobDetails* job_details =
      GetJobDetails(job_id, /*allow_null=*/true);
  if (!job_details)
    return;

  // Update the title, if it's different.
  if (title && job_details->fetch_description->title != *title)
    job_details->fetch_description->title = *title;

  DCHECK(base::Contains(ui_state_map_, job_id));
  UiState& ui_state = ui_state_map_[job_id];

  if (icon) {
    job_details->fetch_description->icon = *icon;
    offline_items_collection::UpdateDelta update_delta;
    update_delta.visuals_changed = true;
    ui_state.update_delta = update_delta;
  }

  bool should_update_visuals = ui_state.update_delta.has_value()
                                   ? ui_state.update_delta->visuals_changed
                                   : false;
#if !BUILDFLAG(IS_ANDROID)
  should_update_visuals = false;
#endif

  if (!should_update_visuals) {
    // Notify the client that the UI updates have been handed over.
    if (job_details->client)
      job_details->client->OnUIUpdated(job_id);
  }

  DoUpdateUi(job_id);
}

void BackgroundFetchDelegateImpl::OpenItem(
    const offline_items_collection::OpenParams& open_params,
    const offline_items_collection::ContentId& id) {
  OnUiActivated(id.id);

  auto* job_details = GetJobDetails(id.id, /*allow_null=*/true);
  if (job_details && job_details->IsComplete())
    OnUiFinished(id.id);
}

void BackgroundFetchDelegateImpl::RemoveItem(
    const offline_items_collection::ContentId& id) {
  // TODO(delphick): Support removing items. (Not sure when this would actually
  // get called though).
  NOTIMPLEMENTED();
}

void BackgroundFetchDelegateImpl::CancelDownload(
    const offline_items_collection::ContentId& id) {
  BackgroundFetchDelegateBase::CancelDownload(id.id);
}

void BackgroundFetchDelegateImpl::PauseDownload(
    const offline_items_collection::ContentId& id) {
  UpdateOfflineItem(id.id);
  BackgroundFetchDelegateBase::PauseDownload(id.id);
}

void BackgroundFetchDelegateImpl::ResumeDownload(
    const offline_items_collection::ContentId& id) {
  UpdateOfflineItem(id.id);
  BackgroundFetchDelegateBase::ResumeDownload(id.id);
}

void BackgroundFetchDelegateImpl::GetItemById(
    const offline_items_collection::ContentId& id,
    SingleItemCallback callback) {
  auto iter = ui_state_map_.find(id.id);
  std::optional<offline_items_collection::OfflineItem> offline_item;
  if (iter != ui_state_map_.end())
    offline_item = iter->second.offline_item;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), offline_item));
}

void BackgroundFetchDelegateImpl::GetAllItems(MultipleItemCallback callback) {
  OfflineItemList item_list;
  for (auto& entry : ui_state_map_)
    item_list.push_back(entry.second.offline_item);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), item_list));
}

void BackgroundFetchDelegateImpl::GetVisualsForItem(
    const offline_items_collection::ContentId& id,
    GetVisualsOptions options,
    VisualsCallback callback) {
  if (!options.get_icon) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, nullptr));
    return;
  }
  // GetVisualsForItem mustn't be called directly since offline_items_collection
  // is not re-entrant and it must be called even if there are no visuals.
  auto visuals =
      std::make_unique<offline_items_collection::OfflineItemVisuals>();
  background_fetch::JobDetails* job_details =
      GetJobDetails(id.id, /*allow_null=*/true);
  if (job_details) {
    visuals->icon =
        gfx::Image::CreateFrom1xBitmap(job_details->fetch_description->icon);
    if (job_details->client &&
        job_details->job_state ==
            background_fetch::JobDetails::State::kDownloadsComplete) {
      job_details->client->OnUIUpdated(id.id);
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), id, std::move(visuals)));
}

void BackgroundFetchDelegateImpl::GetShareInfoForItem(
    const offline_items_collection::ContentId& id,
    ShareCallback callback) {
  // TODO(xingliu): Provide OfflineItemShareInfo to |callback|.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), id,
                                nullptr /* OfflineItemShareInfo */));
}

void BackgroundFetchDelegateImpl::RenameItem(
    const offline_items_collection::ContentId& id,
    const std::string& name,
    RenameCallback callback) {
  NOTIMPLEMENTED();
}

download::BackgroundDownloadService*
BackgroundFetchDelegateImpl::GetDownloadService() {
  return BackgroundDownloadServiceFactory::GetInstance()->GetForKey(
      profile_->GetProfileKey());
}

void BackgroundFetchDelegateImpl::OnJobDetailsCreated(
    const std::string& job_id) {
  DCHECK(!base::Contains(ui_state_map_, job_id));
  UiState& ui_state = ui_state_map_[job_id];
  offline_items_collection::OfflineItem offline_item(
      offline_items_collection::ContentId(provider_namespace_, job_id));
  offline_item.creation_time = base::Time::Now();
  offline_item.is_off_the_record = profile_->IsOffTheRecord();
#if BUILDFLAG(IS_ANDROID)
  if (profile_->IsOffTheRecord())
    offline_item.otr_profile_id = profile_->GetOTRProfileID().Serialize();
#endif
  offline_item.original_url =
      GetJobDetails(job_id)->fetch_description->origin.GetURL();
  ui_state.offline_item = offline_item;
  UpdateOfflineItem(job_id);
}

void BackgroundFetchDelegateImpl::DoShowUi(const std::string& job_id) {
  NotifyItemsAdded({ui_state_map_[job_id].offline_item});
}

// Much of the code in offline_item_collection is not re-entrant, so this should
// not be called from any of the OfflineContentProvider-inherited methods.
void BackgroundFetchDelegateImpl::DoUpdateUi(const std::string& job_id) {
  // Update the OfflineItem that controls the contents of download
  // notifications and notify any OfflineContentProvider::Observer that was
  // registered with this instance.
  UpdateOfflineItem(job_id);

  if (ui_state_map_.find(job_id) == ui_state_map_.end()) {
    // This is a delayed update event. The Background Fetch has already
    // completed.
    return;
  }
  UiState& ui_state = ui_state_map_[job_id];
  auto update_delta = std::move(ui_state.update_delta);
  NotifyItemUpdated(ui_state.offline_item, update_delta);
}

void BackgroundFetchDelegateImpl::DoCleanUpUi(const std::string& job_id) {
  ui_state_map_.erase(job_id);
  // Note that the entry in `ui_state_map_` will leak if OnUiFinished is never
  // called, and it's not called when the notification is dismissed without
  // being clicked. See crbug.com/1190390
}

void BackgroundFetchDelegateImpl::UpdateOfflineItem(const std::string& job_id) {
  background_fetch::JobDetails* job_details =
      GetJobDetails(job_id, /*allow_null=*/true);
  if (!job_details)
    return;

  content::BackgroundFetchDescription* fetch_description =
      job_details->fetch_description.get();
  DCHECK_GT(fetch_description->total_requests, 0);

  offline_items_collection::OfflineItem* offline_item =
      &ui_state_map_[job_id].offline_item;

  using JobState = background_fetch::JobDetails::State;
  if (job_details->ShouldReportProgressBySize()) {
    offline_item->progress.value = job_details->GetProcessedBytes();
    // If we have completed all downloads, update progress max to the processed
    // bytes in case the provided totals were set too high. This avoids
    // unnecessary jumping in the progress bar.
    uint64_t completed_bytes =
        fetch_description->downloaded_bytes + fetch_description->uploaded_bytes;
    uint64_t total_bytes = fetch_description->download_total_bytes +
                           fetch_description->upload_total_bytes;
    offline_item->progress.max =
        job_details->job_state == JobState::kDownloadsComplete ? completed_bytes
                                                               : total_bytes;
  } else {
    offline_item->progress.value = fetch_description->completed_requests;
    offline_item->progress.max = fetch_description->total_requests;
  }

  offline_item->progress.unit =
      offline_items_collection::OfflineItemProgressUnit::PERCENTAGE;

  offline_item->title = fetch_description->title;
  offline_item->promote_origin = true;
  offline_item->is_transient = true;
  offline_item->is_resumable = true;

  using OfflineItemState = offline_items_collection::OfflineItemState;
  switch (job_details->job_state) {
    case JobState::kCancelled:
      offline_item->state = OfflineItemState::CANCELLED;
      break;
    case JobState::kDownloadsComplete:
      // This includes cases when the download failed, or completed but the
      // response was an HTTP error, e.g. 404.
      offline_item->state = OfflineItemState::COMPLETE;
      offline_item->is_openable = true;
      break;
    case JobState::kPendingWillStartPaused:
    case JobState::kStartedButPaused:
      offline_item->state = OfflineItemState::PAUSED;
      break;
    case JobState::kJobComplete:
      // There shouldn't be any updates at this point.
      NOTREACHED_IN_MIGRATION();
      break;
    default:
      offline_item->state = OfflineItemState::IN_PROGRESS;
  }
}

void BackgroundFetchDelegateImpl::
    RecordBackgroundFetchDeletingRegistrationUkmEvent(
        const url::Origin& origin,
        bool user_initiated_abort) {
  auto* ukm_background_service =
      ukm::UkmBackgroundRecorderFactory::GetForProfile(profile_);
  ukm_background_service->GetBackgroundSourceIdIfAllowed(
      origin,
      base::BindOnce(&BackgroundFetchDelegateImpl::DidGetBackgroundSourceId,
                     weak_ptr_factory_.GetWeakPtr(), user_initiated_abort));
}

void BackgroundFetchDelegateImpl::DidGetBackgroundSourceId(
    bool user_initiated_abort,
    std::optional<ukm::SourceId> source_id) {
  // This background event did not meet the requirements for the UKM service.
  if (!source_id)
    return;

  ukm::builders::BackgroundFetchDeletingRegistration(*source_id)
      .SetUserInitiatedAbort(user_initiated_abort)
      .Record(ukm::UkmRecorder::Get());
}

BackgroundFetchDelegateImpl::UiState::UiState() = default;
BackgroundFetchDelegateImpl::UiState::~UiState() = default;
