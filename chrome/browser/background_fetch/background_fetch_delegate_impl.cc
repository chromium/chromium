// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/common/download_features.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

BackgroundFetchDelegateImpl::BackgroundFetchDelegateImpl(
    Profile* profile,
    const std::string& provider_namespace)
    : profile_(profile),
      provider_namespace_(provider_namespace),
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
    content::BrowserContext::GetDownloadManager(profile_);
  }
}

BackgroundFetchDelegateImpl::~BackgroundFetchDelegateImpl() {
  offline_content_aggregator_->UnregisterProvider(provider_namespace_);
}

download::DownloadService* BackgroundFetchDelegateImpl::GetDownloadService() {
  if (download_service_)
    return download_service_;

  download_service_ = DownloadServiceFactory::GetInstance()->GetForKey(
      profile_->GetProfileKey());
  return download_service_;
}

void BackgroundFetchDelegateImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

BackgroundFetchDelegateImpl::JobDetails::RequestData::RequestData(
    bool has_upload_data) {
  if (has_upload_data)
    status = Status::kIncluded;
  else
    status = Status::kAbsent;
}

BackgroundFetchDelegateImpl::JobDetails::RequestData::~RequestData() = default;

BackgroundFetchDelegateImpl::JobDetails::JobDetails(JobDetails&&) = default;

BackgroundFetchDelegateImpl::JobDetails::JobDetails(
    base::WeakPtr<Client> client,
    std::unique_ptr<content::BackgroundFetchDescription> fetch_description,
    const std::string& provider_namespace,
    bool is_off_the_record)
    : client(std::move(client)),
      offline_item(offline_items_collection::ContentId(
          provider_namespace,
          fetch_description->job_unique_id)),
      job_state(fetch_description->start_paused
                    ? State::kPendingWillStartPaused
                    : State::kPendingWillStartDownloading),
      fetch_description(std::move(fetch_description)) {
  offline_item.is_off_the_record = is_off_the_record;
  offline_item.original_url = this->fetch_description->origin.GetURL();
  UpdateOfflineItem();
}

BackgroundFetchDelegateImpl::JobDetails::~JobDetails() = default;

void BackgroundFetchDelegateImpl::JobDetails::MarkJobAsStarted() {
  if (job_state == State::kPendingWillStartDownloading)
    job_state = State::kStartedAndDownloading;
  else if (job_state == State::kPendingWillStartPaused)
    job_state = State::kStartedButPaused;
}

void BackgroundFetchDelegateImpl::JobDetails::UpdateJobOnDownloadComplete(
    const std::string& download_guid) {
  fetch_description->completed_requests++;
  if (fetch_description->completed_requests ==
      fetch_description->total_requests) {
    job_state = State::kDownloadsComplete;
  }

  current_fetch_guids.erase(download_guid);
}

void BackgroundFetchDelegateImpl::JobDetails::UpdateOfflineItem() {
  DCHECK_GT(fetch_description->total_requests, 0);

  if (ShouldReportProgressBySize()) {
    offline_item.progress.value = GetProcessedBytes();
    // If we have completed all downloads, update progress max to the processed
    // bytes in case the provided totals were set too high. This avoids
    // unnecessary jumping in the progress bar.
    uint64_t completed_bytes =
        fetch_description->downloaded_bytes + fetch_description->uploaded_bytes;
    uint64_t total_bytes = fetch_description->download_total_bytes +
                           fetch_description->upload_total_bytes;
    offline_item.progress.max =
        job_state == State::kDownloadsComplete ? completed_bytes : total_bytes;
  } else {
    offline_item.progress.value = fetch_description->completed_requests;
    offline_item.progress.max = fetch_description->total_requests;
  }

  offline_item.progress.unit =
      offline_items_collection::OfflineItemProgressUnit::PERCENTAGE;

  offline_item.title = fetch_description->title;
  offline_item.promote_origin = true;
  offline_item.is_transient = true;
  offline_item.is_resumable = true;

  using OfflineItemState = offline_items_collection::OfflineItemState;
  switch (job_state) {
    case State::kCancelled:
      offline_item.state = OfflineItemState::CANCELLED;
      break;
    case State::kDownloadsComplete:
      // This includes cases when the download failed, or completed but the
      // response was an HTTP error, e.g. 404.
      offline_item.state = OfflineItemState::COMPLETE;
      offline_item.is_openable = true;
      break;
    case State::kPendingWillStartPaused:
    case State::kStartedButPaused:
      offline_item.state = OfflineItemState::PAUSED;
      break;
    case State::kJobComplete:
      // There shouldn't be any updates at this point.
      NOTREACHED();
      break;
    default:
      offline_item.state = OfflineItemState::IN_PROGRESS;
  }
}

uint64_t BackgroundFetchDelegateImpl::JobDetails::GetProcessedBytes() const {
  return fetch_description->downloaded_bytes +
         fetch_description->uploaded_bytes + GetInProgressBytes();
}

uint64_t BackgroundFetchDelegateImpl::JobDetails::GetDownloadedBytes() const {
  uint64_t bytes = fetch_description->downloaded_bytes;
  for (const auto& current_fetch : current_fetch_guids)
    bytes += current_fetch.second.in_progress_downloaded_bytes;
  return bytes;
}

uint64_t BackgroundFetchDelegateImpl::JobDetails::GetInProgressBytes() const {
  uint64_t bytes = 0u;
  for (const auto& current_fetch : current_fetch_guids) {
    bytes += current_fetch.second.in_progress_downloaded_bytes +
             current_fetch.second.in_progress_uploaded_bytes;
  }
  return bytes;
}

void BackgroundFetchDelegateImpl::JobDetails::UpdateInProgressBytes(
    const std::string& download_guid,
    uint64_t bytes_uploaded,
    uint64_t bytes_downloaded) {
  DCHECK(current_fetch_guids.count(download_guid));
  auto& request_data = current_fetch_guids.find(download_guid)->second;

  // If we started receiving download bytes then the upload was complete and is
  // accounted for in |uploaded_bytes|.
  if (bytes_downloaded > 0u)
    request_data.in_progress_uploaded_bytes = 0u;
  else
    request_data.in_progress_uploaded_bytes = bytes_uploaded;

  request_data.in_progress_downloaded_bytes = bytes_downloaded;
}

bool BackgroundFetchDelegateImpl::JobDetails::ShouldReportProgressBySize() {
  if (!fetch_description->download_total_bytes) {
    // |download_total_bytes| was not set. Cannot report by size.
    return false;
  }

  if (fetch_description->completed_requests <
          fetch_description->total_requests &&
      GetDownloadedBytes() > fetch_description->download_total_bytes) {
    // |download_total_bytes| was set too low.
    return false;
  }

  return true;
}

void BackgroundFetchDelegateImpl::GetIconDisplaySize(
    BackgroundFetchDelegate::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If Android, return 192x192, else return 0x0. 0x0 means not loading an
  // icon at all, which is returned for all non-Android platforms as the
  // icons can't be displayed on the UI yet.
  // TODO(nator): Move this logic to OfflineItemsCollection, and return icon
  // size based on display.
  gfx::Size display_size;
#if defined(OS_ANDROID)
  display_size = gfx::Size(192, 192);
#endif
  std::move(callback).Run(display_size);
}

void BackgroundFetchDelegateImpl::GetPermissionForOrigin(
    const url::Origin& origin,
    const content::WebContents::Getter& wc_getter,
    GetPermissionForOriginCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (wc_getter) {
    // There is an associated frame so we might need to expose some permission
    // UI using the DownloadRequestLimiter.
    DownloadRequestLimiter* limiter =
        g_browser_process->download_request_limiter();
    DCHECK(limiter);

    // The fetch should be thought of as one download. So the origin will be
    // used as the URL, and the |request_method| is set to GET.
    limiter->CanDownload(
        wc_getter, origin.GetURL(), "GET", base::nullopt,
        base::AdaptCallbackForRepeating(base::BindOnce(
            &BackgroundFetchDelegateImpl::
                DidGetPermissionFromDownloadRequestLimiter,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
    return;
  }

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  // This is running from a non-top level frame, use the Automatic Downloads
  // content setting.
  ContentSetting content_setting = host_content_settings_map->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::AUTOMATIC_DOWNLOADS,
      std::string() /* resource_identifier */);

  // The set of valid settings for automatic downloads is set to
  // {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK}.
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_ASK:
      std::move(callback).Run(content::BackgroundFetchPermission::ASK);
      return;
    case CONTENT_SETTING_BLOCK:
      std::move(callback).Run(content::BackgroundFetchPermission::BLOCKED);
      return;
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_SESSION_ONLY:
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
  }
}

void BackgroundFetchDelegateImpl::DidGetPermissionFromDownloadRequestLimiter(
    GetPermissionForOriginCallback callback,
    bool has_permission) {
  std::move(callback).Run(has_permission
                              ? content::BackgroundFetchPermission::ALLOWED
                              : content::BackgroundFetchPermission::BLOCKED);
}

void BackgroundFetchDelegateImpl::CreateDownloadJob(
    base::WeakPtr<Client> client,
    std::unique_ptr<content::BackgroundFetchDescription> fetch_description) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string job_unique_id = fetch_description->job_unique_id;
  DCHECK(!job_details_map_.count(job_unique_id));
  job_details_map_.emplace(
      job_unique_id,
      JobDetails(std::move(client), std::move(fetch_description),
                 provider_namespace_, profile_->IsOffTheRecord()));
}

void BackgroundFetchDelegateImpl::DownloadUrl(
    const std::string& job_unique_id,
    const std::string& download_guid,
    const std::string& method,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const net::HttpRequestHeaders& headers,
    bool has_request_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(job_details_map_.count(job_unique_id));
  DCHECK(!download_job_unique_id_map_.count(download_guid));

  download_job_unique_id_map_.emplace(download_guid, job_unique_id);

  download::DownloadParams params;
  params.guid = download_guid;
  params.client = download::DownloadClient::BACKGROUND_FETCH;
  params.request_params.method = method;
  params.request_params.url = url;
  params.request_params.request_headers = headers;
  params.callback = base::Bind(&BackgroundFetchDelegateImpl::OnDownloadReceived,
                               weak_ptr_factory_.GetWeakPtr());
  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation);

  JobDetails& job_details = job_details_map_.find(job_unique_id)->second;

  if (job_details.job_state == JobDetails::State::kPendingWillStartPaused ||
      job_details.job_state ==
          JobDetails::State::kPendingWillStartDownloading) {
    // Create a notification.
    for (auto* observer : observers_)
      observer->OnItemsAdded({job_details.offline_item});
    job_details.MarkJobAsStarted();
  }

  if (job_details.job_state == JobDetails::State::kStartedButPaused) {
    job_details.on_resume =
        base::BindOnce(&BackgroundFetchDelegateImpl::StartDownload,
                       GetWeakPtr(), job_unique_id, params, has_request_body);
  } else {
    StartDownload(job_unique_id, params, has_request_body);
  }

  UpdateOfflineItemAndUpdateObservers(&job_details);
}

void BackgroundFetchDelegateImpl::StartDownload(
    const std::string& job_unique_id,
    const download::DownloadParams& params,
    bool has_request_body) {
  DCHECK(job_details_map_.count(job_unique_id));
  JobDetails& job_details = job_details_map_.find(job_unique_id)->second;

  job_details.current_fetch_guids.emplace(params.guid, has_request_body);
  GetDownloadService()->StartDownload(params);
}

void BackgroundFetchDelegateImpl::Abort(const std::string& job_unique_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto job_details_iter = job_details_map_.find(job_unique_id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;
  job_details.job_state = JobDetails::State::kCancelled;

  for (const auto& download_guid_pair : job_details.current_fetch_guids) {
    GetDownloadService()->CancelDownload(download_guid_pair.first);
    download_job_unique_id_map_.erase(download_guid_pair.first);
  }
  UpdateOfflineItemAndUpdateObservers(&job_details);
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
    base::Optional<ukm::SourceId> source_id) {
  // This background event did not meet the requirements for the UKM service.
  if (!source_id)
    return;

  ukm::builders::BackgroundFetchDeletingRegistration(*source_id)
      .SetUserInitiatedAbort(user_initiated_abort)
      .Record(ukm::UkmRecorder::Get());

  if (ukm_event_recorded_for_testing_)
    std::move(ukm_event_recorded_for_testing_).Run();
}

void BackgroundFetchDelegateImpl::MarkJobComplete(
    const std::string& job_unique_id) {
  auto job_details_iter = job_details_map_.find(job_unique_id);
  DCHECK(job_details_iter != job_details_map_.end());

  JobDetails& job_details = job_details_iter->second;
  job_details.job_state = JobDetails::State::kJobComplete;

  RecordBackgroundFetchDeletingRegistrationUkmEvent(
      job_details.fetch_description->origin, job_details.cancelled_from_ui);

  // Clear the |job_details| internals that are no longer needed.
  job_details.current_fetch_guids.clear();
  job_details.fetch_description.reset();
}

void BackgroundFetchDelegateImpl::UpdateUI(
    const std::string& job_unique_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(title || icon);             // One of the UI options must be updatable.
  DCHECK(!icon || !icon->isNull());  // The |icon|, if provided, is not null.

  auto job_details_iter = job_details_map_.find(job_unique_id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;
  // Update the title, if it's different.
  if (title && job_details.fetch_description->title != *title)
    job_details.fetch_description->title = *title;

  if (icon) {
    job_details.fetch_description->icon = *icon;
    offline_items_collection::UpdateDelta update_delta;
    update_delta.visuals_changed = true;
    job_details.update_delta = update_delta;
  }

  bool should_update_visuals = job_details.update_delta.has_value()
                                   ? job_details.update_delta->visuals_changed
                                   : false;
#if !defined(OS_ANDROID)
  should_update_visuals = false;
#endif

  if (!should_update_visuals) {
    // Notify the client that the UI updates have been handed over.
    if (job_details.client)
      job_details.client->OnUIUpdated(job_unique_id);
  }

  UpdateOfflineItemAndUpdateObservers(&job_details);
}

void BackgroundFetchDelegateImpl::OnDownloadStarted(
    const std::string& download_guid,
    std::unique_ptr<content::BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto download_job_unique_id_iter =
      download_job_unique_id_map_.find(download_guid);
  // TODO(crbug.com/779012): When DownloadService fixes cancelled jobs calling
  // OnDownload* methods, then this can be a DCHECK.
  if (download_job_unique_id_iter == download_job_unique_id_map_.end())
    return;

  const std::string& job_unique_id = download_job_unique_id_iter->second;
  auto& job_details = job_details_map_.find(job_unique_id)->second;
  if (job_details.client) {
    job_details.client->OnDownloadStarted(job_unique_id, download_guid,
                                          std::move(response));
  }

  // Update the upload progress.
  auto it = job_details.current_fetch_guids.find(download_guid);
  DCHECK(it != job_details.current_fetch_guids.end());
  job_details.fetch_description->uploaded_bytes += it->second.body_size_bytes;
}

void BackgroundFetchDelegateImpl::OnDownloadUpdated(
    const std::string& download_guid,
    uint64_t bytes_uploaded,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto download_job_unique_id_iter =
      download_job_unique_id_map_.find(download_guid);
  // TODO(crbug.com/779012): When DownloadService fixes cancelled jobs calling
  // OnDownload* methods, then this can be a DCHECK.
  if (download_job_unique_id_iter == download_job_unique_id_map_.end())
    return;

  const std::string& job_unique_id = download_job_unique_id_iter->second;

  DCHECK(job_details_map_.count(job_unique_id));
  JobDetails& job_details = job_details_map_.find(job_unique_id)->second;

  job_details.UpdateInProgressBytes(download_guid, bytes_uploaded,
                                    bytes_downloaded);
  if (job_details.fetch_description->download_total_bytes &&
      job_details.fetch_description->download_total_bytes <
          job_details.GetDownloadedBytes()) {
    // Fail the fetch if total download size was set too low.
    // We only do this if total download size is specified. If not specified,
    // this check is skipped. This is to allow for situations when the
    // total download size cannot be known when invoking fetch.
    FailFetch(job_unique_id);
    return;
  }
  UpdateOfflineItemAndUpdateObservers(&job_details);

  if (job_details.client) {
    job_details.client->OnDownloadUpdated(job_unique_id, download_guid,
                                          bytes_uploaded, bytes_downloaded);
  }
}

void BackgroundFetchDelegateImpl::OnDownloadFailed(
    const std::string& download_guid,
    std::unique_ptr<content::BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto download_job_unique_id_iter =
      download_job_unique_id_map_.find(download_guid);
  // TODO(crbug.com/779012): When DownloadService fixes cancelled jobs
  // potentially calling OnDownloadFailed with a reason other than
  // CANCELLED/ABORTED, we should add a DCHECK here.
  if (download_job_unique_id_iter == download_job_unique_id_map_.end())
    return;

  const std::string& job_unique_id = download_job_unique_id_iter->second;
  JobDetails& job_details = job_details_map_.find(job_unique_id)->second;

  job_details.UpdateJobOnDownloadComplete(download_guid);
  UpdateOfflineItemAndUpdateObservers(&job_details);

  // The client cancelled or aborted the download so no need to notify it.
  if (result->failure_reason ==
      content::BackgroundFetchResult::FailureReason::CANCELLED) {
    return;
  }

  if (job_details.client) {
    job_details.client->OnDownloadComplete(job_unique_id, download_guid,
                                           std::move(result));
  }

  download_job_unique_id_map_.erase(download_guid);
}

void BackgroundFetchDelegateImpl::OnDownloadSucceeded(
    const std::string& download_guid,
    std::unique_ptr<content::BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto download_job_unique_id_iter =
      download_job_unique_id_map_.find(download_guid);
  // TODO(crbug.com/779012): When DownloadService fixes cancelled jobs calling
  // OnDownload* methods, then this can be a DCHECK.
  if (download_job_unique_id_iter == download_job_unique_id_map_.end())
    return;

  const std::string& job_unique_id = download_job_unique_id_iter->second;
  JobDetails& job_details = job_details_map_.find(job_unique_id)->second;
  job_details.UpdateJobOnDownloadComplete(download_guid);

  job_details.fetch_description->downloaded_bytes +=
      profile_->IsOffTheRecord() ? result->blob_handle->size()
                                 : result->file_size;

  UpdateOfflineItemAndUpdateObservers(&job_details);

  if (job_details.client) {
    job_details.client->OnDownloadComplete(job_unique_id, download_guid,
                                           std::move(result));
  }

  download_job_unique_id_map_.erase(download_guid);
}

void BackgroundFetchDelegateImpl::OnDownloadReceived(
    const std::string& download_guid,
    download::DownloadParams::StartResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using StartResult = download::DownloadParams::StartResult;
  switch (result) {
    case StartResult::ACCEPTED:
      // Nothing to do.
      break;
    case StartResult::UNEXPECTED_GUID:
      // The download started in a previous session. Nothing to do.
      break;
    case StartResult::BACKOFF:
      // TODO(delphick): try again later?
      NOTREACHED();
      break;
    case StartResult::UNEXPECTED_CLIENT:
      // This really should never happen since we're supplying the
      // DownloadClient.
      NOTREACHED();
      break;
    case StartResult::CLIENT_CANCELLED:
      // TODO(delphick): do we need to do anything here, since we will have
      // cancelled it?
      break;
    case StartResult::INTERNAL_ERROR:
      // TODO(delphick): We need to handle this gracefully.
      NOTREACHED();
      break;
    case StartResult::COUNT:
      NOTREACHED();
      break;
  }
}

// Much of the code in offline_item_collection is not re-entrant, so this should
// not be called from any of the OfflineContentProvider-inherited methods.
void BackgroundFetchDelegateImpl::UpdateOfflineItemAndUpdateObservers(
    JobDetails* job_details) {
  job_details->UpdateOfflineItem();

  auto update_delta = std::move(job_details->update_delta);
  for (auto* observer : observers_)
    observer->OnItemUpdated(job_details->offline_item, update_delta);
}

void BackgroundFetchDelegateImpl::OpenItem(
    offline_items_collection::LaunchLocation location,
    const offline_items_collection::ContentId& id) {
  auto job_details_iter = job_details_map_.find(id.id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;
  if (job_details.client)
    job_details.client->OnUIActivated(id.id);

  // No point in keeping the job details around anymore.
  if (job_details.job_state == JobDetails::State::kJobComplete)
    job_details_map_.erase(job_details_iter);
}

void BackgroundFetchDelegateImpl::RemoveItem(
    const offline_items_collection::ContentId& id) {
  // TODO(delphick): Support removing items. (Not sure when this would actually
  // get called though).
  NOTIMPLEMENTED();
}

void BackgroundFetchDelegateImpl::FailFetch(const std::string& job_unique_id) {
  // Save a copy before Abort() deletes the reference.
  const std::string unique_id = job_unique_id;
  Abort(job_unique_id);

  if (auto client = GetClient(unique_id)) {
    client->OnJobCancelled(
        unique_id,
        blink::mojom::BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED);
  }
}

void BackgroundFetchDelegateImpl::CancelDownload(
    const offline_items_collection::ContentId& id) {
  // Save a copy before Abort() deletes the reference.
  const std::string unique_id = id.id;

  auto job_details_iter = job_details_map_.find(unique_id);
  DCHECK(job_details_iter != job_details_map_.end());

  auto& job_details = job_details_iter->second;
  if (job_details.job_state == JobDetails::State::kDownloadsComplete ||
      job_details.job_state == JobDetails::State::kJobComplete) {
    // The cancel event arrived after the fetch was complete; ignore it.
    return;
  }

  job_details.cancelled_from_ui = true;
  Abort(unique_id);

  if (auto client = GetClient(unique_id)) {
    client->OnJobCancelled(
        unique_id,
        blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI);
  }
}

void BackgroundFetchDelegateImpl::PauseDownload(
    const offline_items_collection::ContentId& id) {
  auto job_details_iter = job_details_map_.find(id.id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;
  if (job_details.job_state == JobDetails::State::kDownloadsComplete ||
      job_details.job_state == JobDetails::State::kJobComplete) {
    // The pause event arrived after the fetch was complete; ignore it.
    return;
  }

  job_details.job_state = JobDetails::State::kStartedButPaused;
  job_details.UpdateOfflineItem();
  for (auto& download_guid_pair : job_details.current_fetch_guids)
    GetDownloadService()->PauseDownload(download_guid_pair.first);
}

void BackgroundFetchDelegateImpl::ResumeDownload(
    const offline_items_collection::ContentId& id,
    bool has_user_gesture) {
  auto job_details_iter = job_details_map_.find(id.id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;
  job_details.job_state = JobDetails::State::kStartedAndDownloading;
  job_details.UpdateOfflineItem();
  for (auto& download_guid_pair : job_details.current_fetch_guids)
    GetDownloadService()->ResumeDownload(download_guid_pair.first);

  if (job_details.on_resume)
    std::move(job_details.on_resume).Run();
}

void BackgroundFetchDelegateImpl::GetItemById(
    const offline_items_collection::ContentId& id,
    SingleItemCallback callback) {
  auto it = job_details_map_.find(id.id);
  base::Optional<offline_items_collection::OfflineItem> offline_item;
  if (it != job_details_map_.end())
    offline_item = it->second.offline_item;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), offline_item));
}

void BackgroundFetchDelegateImpl::GetAllItems(MultipleItemCallback callback) {
  OfflineItemList item_list;
  for (auto& entry : job_details_map_)
    item_list.push_back(entry.second.offline_item);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), item_list));
}

void BackgroundFetchDelegateImpl::GetVisualsForItem(
    const offline_items_collection::ContentId& id,
    GetVisualsOptions options,
    VisualsCallback callback) {
  if (!options.get_icon) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, nullptr));
    return;
  }
  // GetVisualsForItem mustn't be called directly since offline_items_collection
  // is not re-entrant and it must be called even if there are no visuals.
  auto visuals =
      std::make_unique<offline_items_collection::OfflineItemVisuals>();
  auto it = job_details_map_.find(id.id);
  if (it != job_details_map_.end()) {
    auto& job_details = it->second;
    visuals->icon =
        gfx::Image::CreateFrom1xBitmap(job_details.fetch_description->icon);
    if (job_details.client &&
        job_details.job_state == JobDetails::State::kDownloadsComplete) {
      job_details.client->OnUIUpdated(id.id);
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), id, std::move(visuals)));
}

void BackgroundFetchDelegateImpl::GetShareInfoForItem(
    const offline_items_collection::ContentId& id,
    ShareCallback callback) {
  // TODO(xingliu): Provide OfflineItemShareInfo to |callback|.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), id,
                                nullptr /* OfflineItemShareInfo */));
}

void BackgroundFetchDelegateImpl::RenameItem(
    const offline_items_collection::ContentId& id,
    const std::string& name,
    RenameCallback callback) {
  NOTIMPLEMENTED();
}

void BackgroundFetchDelegateImpl::AddObserver(Observer* observer) {
  DCHECK(!observers_.count(observer));

  observers_.insert(observer);
}

void BackgroundFetchDelegateImpl::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}

bool BackgroundFetchDelegateImpl::IsGuidOutstanding(
    const std::string& guid) const {
  auto unique_id_it = download_job_unique_id_map_.find(guid);
  if (unique_id_it == download_job_unique_id_map_.end())
    return false;

  auto job_details_it = job_details_map_.find(unique_id_it->second);
  if (job_details_it == job_details_map_.end())
    return false;

  std::vector<std::string>& outstanding_guids =
      job_details_it->second.fetch_description->outstanding_guids;
  return std::find(outstanding_guids.begin(), outstanding_guids.end(), guid) !=
         outstanding_guids.end();
}

void BackgroundFetchDelegateImpl::RestartPausedDownload(
    const std::string& download_guid) {
  auto job_it = download_job_unique_id_map_.find(download_guid);

  if (job_it == download_job_unique_id_map_.end())
    return;

  const std::string& unique_id = job_it->second;

  DCHECK(job_details_map_.find(unique_id) != job_details_map_.end());
  JobDetails& job_details = job_details_map_.find(unique_id)->second;
  job_details.job_state = JobDetails::State::kStartedButPaused;

  UpdateOfflineItemAndUpdateObservers(&job_details);
}

std::set<std::string> BackgroundFetchDelegateImpl::TakeOutstandingGuids() {
  std::set<std::string> outstanding_guids;
  for (auto& job_id_details : job_details_map_) {
    auto& job_details = job_id_details.second;

    // If the job is loaded at this point, then it already started
    // in a previous session.
    job_details.MarkJobAsStarted();

    std::vector<std::string>& job_outstanding_guids =
        job_details.fetch_description->outstanding_guids;
    for (std::string& outstanding_guid : job_outstanding_guids)
      outstanding_guids.insert(std::move(outstanding_guid));
    job_outstanding_guids.clear();
  }
  return outstanding_guids;
}

void BackgroundFetchDelegateImpl::GetUploadData(
    const std::string& download_guid,
    download::GetUploadDataCallback callback) {
  auto job_it = download_job_unique_id_map_.find(download_guid);
  DCHECK(job_it != download_job_unique_id_map_.end());

  JobDetails& job_details = job_details_map_.find(job_it->second)->second;
  if (job_details.current_fetch_guids.at(download_guid).status ==
      JobDetails::RequestData::Status::kAbsent) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /* request_body= */ nullptr));
    return;
  }

  if (job_details.client) {
    job_details.client->GetUploadData(
        job_it->second, download_guid,
        base::BindOnce(&BackgroundFetchDelegateImpl::DidGetUploadData,
                       weak_ptr_factory_.GetWeakPtr(), job_it->second,
                       download_guid, std::move(callback)));
  }
}

void BackgroundFetchDelegateImpl::DidGetUploadData(
    const std::string& unique_id,
    const std::string& download_guid,
    download::GetUploadDataCallback callback,
    blink::mojom::SerializedBlobPtr blob) {
  if (!blob || blob->uuid.empty()) {
    std::move(callback).Run(/* request_body= */ nullptr);
    return;
  }

  auto job_it = job_details_map_.find(unique_id);
  if (job_it == job_details_map_.end()) {
    std::move(callback).Run(/* request_body= */ nullptr);
    return;
  }

  auto& job_details = job_it->second;
  DCHECK(job_details.current_fetch_guids.count(download_guid));
  auto& request_data = job_details.current_fetch_guids.at(download_guid);
  request_data.body_size_bytes = blob->size;

  // Use a Data Pipe to transfer the blob.
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter_remote;
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob->blob));
  blob_remote->AsDataPipeGetter(
      data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  auto request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request_body->AppendDataPipe(std::move(data_pipe_getter_remote));

  std::move(callback).Run(request_body);
}

base::WeakPtr<content::BackgroundFetchDelegate::Client>
BackgroundFetchDelegateImpl::GetClient(const std::string& job_unique_id) {
  auto it = job_details_map_.find(job_unique_id);
  if (it == job_details_map_.end())
    return nullptr;
  return it->second.client;
}
