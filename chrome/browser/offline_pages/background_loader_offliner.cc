// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/offline_pages/background_loader_offliner.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/offline_pages/offliner_helper.h"
#include "chrome/browser/offline_pages/offliner_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/mhtml_extra_parts.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"

namespace offline_pages {

namespace {

void HandleLoadTerminationCancel(
    Offliner::CompletionCallback completion_callback,
    const SavePageRequest& canceled_request) {
  std::move(completion_callback)
      .Run(canceled_request, Offliner::RequestStatus::FOREGROUND_CANCELED);
}

}  // namespace

BackgroundLoaderOffliner::BackgroundLoaderOffliner(
    content::BrowserContext* browser_context,
    const OfflinerPolicy* policy,
    OfflinePageModel* offline_page_model,
    std::unique_ptr<LoadTerminationListener> load_termination_listener)
    : browser_context_(browser_context),
      offline_page_model_(offline_page_model),
      policy_(policy),
      load_termination_listener_(std::move(load_termination_listener)),
      save_state_(NONE),
      page_load_state_(SUCCESS),
      network_bytes_(0LL),
      is_low_bar_met_(false),
      did_snapshot_on_last_retry_(false) {
  DCHECK(offline_page_model_);
  DCHECK(browser_context_);
  // When the offliner is created for test harness runs, the
  // |load_termination_listener_| will be set to nullptr, in order to prevent
  // crashing, adding a check here.
  if (load_termination_listener_)
    load_termination_listener_->set_offliner(this);

  for (int i = 0; i < ResourceDataType::RESOURCE_DATA_TYPE_COUNT; ++i) {
    stats_[i].requested = 0;
    stats_[i].completed = 0;
  }
}

BackgroundLoaderOffliner::~BackgroundLoaderOffliner() {}

// static
BackgroundLoaderOffliner* BackgroundLoaderOffliner::FromWebContents(
    content::WebContents* contents) {
  Offliner* offliner = OfflinerUserData::OfflinerFromWebContents(contents);
  // Today we only have one kind of offliner that uses OfflinerUserData.  If we
  // add other types, revisit this cast.
  if (offliner)
    return static_cast<BackgroundLoaderOffliner*>(offliner);
  return nullptr;
}

bool BackgroundLoaderOffliner::LoadAndSave(
    const SavePageRequest& request,
    CompletionCallback completion_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(completion_callback);
  DCHECK(progress_callback);
  DCHECK(offline_page_model_);

  if (pending_request_) {
    DVLOG(1) << "Already have pending request";
    return false;
  }

  if (GetPolicy(request.client_id().name_space)
          .requires_specific_user_settings &&
      (AreThirdPartyCookiesBlocked(browser_context_) ||
       IsNetworkPredictionDisabled(browser_context_))) {
    DVLOG(1) << "WARNING: Unable to load when 3rd party cookies blocked or "
             << "prediction disabled";
    return false;
  }

  if (!OfflinePageModel::CanSaveURL(request.url())) {
    DVLOG(1) << "Not able to save page for requested url: " << request.url();
    return false;
  }

  ResetLoader();
  AttachObservers();

  MarkLoadStartTime();

  // Track copy of pending request.
  pending_request_ = std::make_unique<SavePageRequest>(request);
  completion_callback_ = std::move(completion_callback);
  progress_callback_ = progress_callback;

  // Load page attempt.
  loader_.get()->LoadPage(request.url());

  snapshot_controller_ = std::make_unique<BackgroundSnapshotController>(
      base::SingleThreadTaskRunner::GetCurrentDefault(), this, false);

  return true;
}

bool BackgroundLoaderOffliner::Cancel(CancelCallback callback) {
  DCHECK(pending_request_);
  // We ignore the case where pending_request_ is not set, but given the checks
  // in RequestCoordinator this should not happen.
  if (!pending_request_)
    return false;

  // TODO(chili): We are not able to cancel a pending
  // OfflinePageModel::SaveSnapshot() operation. We will notify caller that
  // cancel completed once the SavePage operation returns.
  if (save_state_ != NONE) {
    save_state_ = DELETE_AFTER_SAVE;
    cancel_callback_ = std::move(callback);
    return true;
  }

  // Post the cancel callback right after this call concludes.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), *pending_request_.get()));
  ResetState();
  return true;
}

void BackgroundLoaderOffliner::TerminateLoadIfInProgress() {
  if (!pending_request_)
    return;

  Cancel(base::BindOnce(HandleLoadTerminationCancel,
                        std::move(completion_callback_)));
}

bool BackgroundLoaderOffliner::HandleTimeout(int64_t request_id) {
  if (pending_request_) {
    DCHECK(request_id == pending_request_->request_id());
    if (is_low_bar_met_ && (pending_request_->started_attempt_count() + 1 >=
                                policy_->GetMaxStartedTries() ||
                            pending_request_->completed_attempt_count() + 1 >=
                                policy_->GetMaxCompletedTries())) {
      // If we are already in the middle of a save operation, let it finish
      // but do not return SAVED_ON_LAST_RETRY
      if (save_state_ == NONE) {
        did_snapshot_on_last_retry_ = true;
        StartSnapshot();
      }
      return true;
    }
  }
  return false;
}

void BackgroundLoaderOffliner::CanDownload(
    base::OnceCallback<void(bool)> callback) {
  if (!pending_request_.get()) {
    std::move(callback).Run(false);  // Shouldn't happen though...
    return;
  }

  bool should_allow_downloads = false;
  Offliner::RequestStatus final_status =
      Offliner::RequestStatus::LOADING_FAILED_DOWNLOAD;
  // Check whether we should allow file downloads for this save page request.
  // If we want to proceed with the file download, fail with
  // DOWNLOAD_THROTTLED. If we don't want to proceed with the file download,
  // fail with LOADING_FAILED_DOWNLOAD.
  if (GetPolicy(pending_request_.get()->client_id().name_space)
          .allows_conversion_to_background_file_download) {
    should_allow_downloads = true;
    final_status = Offliner::RequestStatus::DOWNLOAD_THROTTLED;
  }

  std::move(callback).Run(should_allow_downloads);
  SavePageRequest request(*pending_request_.get());
  std::move(completion_callback_).Run(request, final_status);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundLoaderOffliner::ResetState,
                                weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundLoaderOffliner::MarkLoadStartTime() {
  load_start_time_ = base::TimeTicks::Now();
}

void BackgroundLoaderOffliner::PrimaryMainDocumentElementAvailable() {
  is_low_bar_met_ = true;

  // Add this signal to signal_data_.
  AddLoadingSignal("PrimaryMainDocumentElementAvailable");
}

void BackgroundLoaderOffliner::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (!pending_request_.get()) {
    DVLOG(1) << "DidStopLoading called even though no pending request.";
    return;
  }

  // Add this signal to signal_data_.
  AddLoadingSignal("DocumentOnLoadCompletedInPrimaryMainFrame");

  snapshot_controller_->DocumentOnLoadCompletedInPrimaryMainFrame();
}

void BackgroundLoaderOffliner::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (pending_request_) {
    SavePageRequest request(*pending_request_.get());
    switch (status) {
      case base::TERMINATION_STATUS_OOM:
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
      case base::TERMINATION_STATUS_STILL_RUNNING:
        std::move(completion_callback_)
            .Run(request, Offliner::RequestStatus::LOADING_FAILED_NO_NEXT);
        break;
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      default:
        std::move(completion_callback_)
            .Run(request, Offliner::RequestStatus::LOADING_FAILED);
    }
    ResetState();
  }
}

void BackgroundLoaderOffliner::WebContentsDestroyed() {
  if (pending_request_) {
    SavePageRequest request(*pending_request_.get());
    std::move(completion_callback_)
        .Run(request, Offliner::RequestStatus::LOADING_FAILED);
    ResetState();
  }
}

void BackgroundLoaderOffliner::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;
  // If there was an error of any kind (certificate, client, DNS, etc),
  // Mark as error page. Resetting here causes RecordNavigationMetrics to crash.
  if (navigation_handle->IsErrorPage()) {
    page_load_state_ = RETRIABLE_NET_ERROR;
  } else {
    int status_code = 200;  // Default to OK.
    // No response header can imply intermediate navigation state.
    if (navigation_handle->GetResponseHeaders())
      status_code = navigation_handle->GetResponseHeaders()->response_code();
    // 2XX and 3XX are ok because they indicate success or redirection.
    // We track 301 because it's MOVED_PERMANENTLY and usually accompanies an
    // error page with new address.
    // 400+ codes are client and server errors.
    // We skip 418 because it's a teapot.
    if (status_code == 301 || (status_code >= 400 && status_code != 418)) {
      page_load_state_ = RETRIABLE_HTTP_ERROR;
    }
  }
}

void BackgroundLoaderOffliner::SetBackgroundSnapshotControllerForTest(
    std::unique_ptr<BackgroundSnapshotController> controller) {
  snapshot_controller_ = std::move(controller);
}

void BackgroundLoaderOffliner::ObserveResourceLoading(
    ResourceLoadingObserver::ResourceDataType type,
    bool started) {
  // Add the signal to extra data, and use for tracking.

  RequestStats& found_stats = stats_[type];
  if (started)
    ++found_stats.requested;
  else
    ++found_stats.completed;
}

void BackgroundLoaderOffliner::OnNetworkBytesChanged(int64_t bytes) {
  if (pending_request_ && save_state_ != SAVING) {
    network_bytes_ += bytes;
    progress_callback_.Run(*pending_request_, network_bytes_);
  }
}

void BackgroundLoaderOffliner::StartSnapshot() {
  if (!pending_request_.get()) {
    DVLOG(1) << "Pending request was cleared during delay.";
    return;
  }
  DCHECK(is_low_bar_met_)
      << "Minimum quality must have been reached before saving a snapshot";

  // Add this signal to signal_data_.
  AddLoadingSignal("Snapshotting");

  SavePageRequest request(*pending_request_.get());
  // If there was an error navigating to page, return loading failed.
  if (page_load_state_ != SUCCESS) {
    Offliner::RequestStatus status;
    switch (page_load_state_) {
      case RETRIABLE_NET_ERROR:
        status = Offliner::RequestStatus::LOADING_FAILED_NET_ERROR;
        break;
      case RETRIABLE_HTTP_ERROR:
        status = Offliner::RequestStatus::LOADING_FAILED_HTTP_ERROR;
        break;
      case NONRETRIABLE:
        status = Offliner::RequestStatus::LOADING_FAILED_NO_RETRY;
        break;
      default:
        // We should've already checked for Success before entering here.
        NOTREACHED_IN_MIGRATION();
        status = Offliner::RequestStatus::LOADING_FAILED;
    }

    std::move(completion_callback_).Run(request, status);
    ResetState();
    return;
  }

  content::WebContents* web_contents(
      content::WebContentsObserver::web_contents());

  Offliner::RequestStatus loaded_page_error =
      CanSavePageInBackground(web_contents);
  if (loaded_page_error != Offliner::RequestStatus::UNKNOWN) {
    std::move(completion_callback_).Run(request, loaded_page_error);
    ResetState();
    return;
  }

  save_state_ = SAVING;

  std::unique_ptr<OfflinePageArchiver> archiver(new OfflinePageMHTMLArchiver());

  OfflinePageModel::SavePageParams params;
  params.url = web_contents->GetLastCommittedURL();
  params.client_id = request.client_id();
  params.proposed_offline_id = request.request_id();
  params.is_background = true;
  params.request_origin = request.request_origin();

  // Pass in the original URL if it's different from last committed
  // when redirects occur.
  if (!request.original_url().is_empty())
    params.original_url = request.original_url();
  else if (params.url != request.url())
    params.original_url = request.url();

  offline_page_model_->SavePage(
      params, std::move(archiver), web_contents,
      base::BindOnce(&BackgroundLoaderOffliner::OnPageSaved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundLoaderOffliner::OnPageSaved(SavePageResult save_result,
                                           int64_t offline_id) {
  if (!pending_request_)
    return;

  SavePageRequest request(*pending_request_.get());
  bool did_snapshot_on_last_retry = did_snapshot_on_last_retry_;
  ResetState();

  if (save_state_ == DELETE_AFTER_SAVE) {
    // Delete the saved page off disk and from the OPM.
    PageCriteria criteria;
    criteria.offline_ids = std::vector<int64_t>{offline_id};
    offline_page_model_->DeletePagesWithCriteria(
        criteria,
        base::BindOnce(&BackgroundLoaderOffliner::DeleteOfflinePageCallback,
                       weak_ptr_factory_.GetWeakPtr(), request));
    save_state_ = NONE;
    return;
  }

  save_state_ = NONE;

  Offliner::RequestStatus save_status;
  if (save_result == SavePageResult::ALREADY_EXISTS) {
    save_status = RequestStatus::SAVED;
  } else if (save_result == SavePageResult::SUCCESS) {
    if (did_snapshot_on_last_retry)
      save_status = RequestStatus::SAVED_ON_LAST_RETRY;
    else
      save_status = RequestStatus::SAVED;
  } else {
    save_status = RequestStatus::SAVE_FAILED;
  }

  std::move(completion_callback_).Run(request, save_status);
}

void BackgroundLoaderOffliner::DeleteOfflinePageCallback(
    const SavePageRequest& request,
    DeletePageResult result) {
  std::move(cancel_callback_).Run(request);
}

void BackgroundLoaderOffliner::ResetState() {
  pending_request_.reset();
  // Stop snapshot controller from triggering any more events.
  snapshot_controller_->Stop();
  // Delete the snapshot controller after stack unwinds, so we don't
  // corrupt stack in some edge cases. Deleting it soon should be safe because
  // we check against pending_request_ with every action, and snapshot
  // controller is configured to only call StartSnapshot once for BGL.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, snapshot_controller_.release());
  page_load_state_ = SUCCESS;
  network_bytes_ = 0LL;
  is_low_bar_met_ = false;
  did_snapshot_on_last_retry_ = false;
  content::WebContentsObserver::Observe(nullptr);
  loader_.reset();

  for (int i = 0; i < ResourceDataType::RESOURCE_DATA_TYPE_COUNT; ++i) {
    stats_[i].requested = 0;
    stats_[i].completed = 0;
  }
}

void BackgroundLoaderOffliner::ResetLoader() {
  loader_ = std::make_unique<background_loader::BackgroundLoaderContents>(
      browser_context_);
  loader_->SetDelegate(this);

  // Initialize web contents settings.
  renderer_preferences_util::UpdateFromSystemSettings(
      loader_->web_contents()->GetMutableRendererPrefs(),
      Profile::FromBrowserContext(browser_context_));
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      loader_->web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(
          loader_->web_contents()));
}

void BackgroundLoaderOffliner::AttachObservers() {
  content::WebContents* contents = loader_->web_contents();
  content::WebContentsObserver::Observe(contents);
  OfflinerUserData::CreateForWebContents(contents, this);
}

void BackgroundLoaderOffliner::AddLoadingSignal(const char* signal_name) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  base::TimeDelta delay_so_far = current_time - load_start_time_;
  // We would prefer to use int64_t here, but JSON does not support that type.
  // Given the choice between int and double, we choose to implicitly convert to
  // a double since it maintains more precision (we can get a longer time in
  // milliseconds than we can with a 2 bit int, 53 bits vs 32).
  signal_data_.Set(signal_name, delay_so_far.InMillisecondsF());
}

void BackgroundLoaderOffliner::RenovationsCompleted() {
  snapshot_controller_->RenovationsCompleted();
}

Offliner::RequestStatus BackgroundLoaderOffliner::CanSavePageInBackground(
    content::WebContents* web_contents) {
  DCHECK(is_low_bar_met_)
      << "Minimum quality must have been reached before checking loaded page";
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      GetVisibleSecurityState(web_contents);
  // Checks for HTTPS certificate errors (HTTP connections are not affected).
  if (security_state::HasMajorCertificateError(*visible_security_state))
    return Offliner::RequestStatus::LOADED_PAGE_HAS_CERTIFICATE_ERROR;

  // Checks if the page is blocked by SafeBrowsing.
  if (visible_security_state->malicious_content_status !=
      security_state::MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_NONE) {
    return Offliner::RequestStatus::LOADED_PAGE_IS_BLOCKED;
  }

  // Don't save Chrome error or interstitial pages.
  if (GetPageType(web_contents) != content::PageType::PAGE_TYPE_NORMAL)
    return Offliner::RequestStatus::LOADED_PAGE_IS_CHROME_INTERNAL;

  return Offliner::RequestStatus::UNKNOWN;
}

std::unique_ptr<security_state::VisibleSecurityState>
BackgroundLoaderOffliner::GetVisibleSecurityState(
    content::WebContents* web_contents) {
  // Note: this tab helper needs to be created here as in the background it is
  // not created by default.
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents);
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  return helper->GetVisibleSecurityState();
}

content::PageType BackgroundLoaderOffliner::GetPageType(
    content::WebContents* web_contents) {
  DCHECK(web_contents->GetController().GetVisibleEntry())
      << "An entry must have committed at this WebContents";
  return web_contents->GetController().GetVisibleEntry()->GetPageType();
}

}  // namespace offline_pages
