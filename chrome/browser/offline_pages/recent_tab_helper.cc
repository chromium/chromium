// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/recent_tab_helper.h"

#include <queue>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

namespace {
class DefaultRecentTabHelperDelegate
    : public offline_pages::RecentTabHelper::Delegate {
 public:
  DefaultRecentTabHelperDelegate()
      : is_low_end_device_(base::SysInfo::IsLowEndDevice()) {}

  // offline_pages::RecentTabHelper::Delegate
  std::unique_ptr<offline_pages::OfflinePageArchiver> CreatePageArchiver(
      content::WebContents* web_contents) override {
    return std::make_unique<offline_pages::OfflinePageMHTMLArchiver>();
  }
  bool GetTabId(content::WebContents* web_contents, int* tab_id) override {
    return offline_pages::OfflinePageUtils::GetTabId(web_contents, tab_id);
  }
  bool IsLowEndDevice() override { return is_low_end_device_; }

  bool IsCustomTab(content::WebContents* web_contents) override {
    return offline_pages::OfflinePageUtils::CurrentlyShownInCustomTab(
        web_contents);
  }

 private:
  // Cached value of whether low end device.
  bool is_low_end_device_;
};
}  // namespace

namespace offline_pages {

using PageQuality = SnapshotController::PageQuality;

// Keeps client_id/request_id that will be used for the offline snapshot.
struct RecentTabHelper::SnapshotProgressInfo {
 public:
  // For a downloads snapshot request, where the |request_id| is defined.
  SnapshotProgressInfo(const ClientId& client_id,
                       int64_t request_id,
                       const std::string& origin)
      : client_id(client_id), request_id(request_id), origin(origin) {}

  // For a last_n snapshot request.
  explicit SnapshotProgressInfo(const ClientId& client_id)
      : client_id(client_id) {}

  bool IsForLastN() { return client_id.name_space == kLastNNamespace; }

  // The ClientID to go with the offline page.
  ClientId client_id;

  // Id of the suspended request in Background Offliner. Used to un-suspend
  // the request if the capture of the current page was not possible (e.g.
  // the user navigated to another page before current one was loaded).
  // 0 if this is a "last_n" info.
  int64_t request_id = OfflinePageModel::kInvalidOfflineId;

  // Expected snapshot quality should the saving succeed. This value is only
  // valid for successfully saved snapshots.
  SnapshotController::PageQuality expected_page_quality =
      SnapshotController::PageQuality::POOR;

  // The app that created the tab - either a package name of the CCT origin
  // or empty, meaning chrome.
  std::string origin;
};

RecentTabHelper::RecentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<RecentTabHelper>(*web_contents),
      delegate_(new DefaultRecentTabHelperDelegate()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

RecentTabHelper::~RecentTabHelper() {
}

void RecentTabHelper::SetDelegate(
    std::unique_ptr<RecentTabHelper::Delegate> delegate) {
  DCHECK(delegate);
  delegate_ = std::move(delegate);
}

void RecentTabHelper::ObserveAndDownloadCurrentPage(const ClientId& client_id,
                                                    int64_t request_id,
                                                    const std::string& origin) {
  // Note: as this implementation only supports one client namespace, enforce
  // that the call is from Downloads.
  DCHECK_EQ(kDownloadNamespace, client_id.name_space);
  auto new_downloads_snapshot_info =
      std::make_unique<SnapshotProgressInfo>(client_id, request_id, origin);

  // If this tab helper is not enabled, immediately give the job back to
  // RequestCoordinator.
  if (!EnsureInitialized()) {
    DVLOG(1) << "Snapshots disabled; ignored download request for: "
             << web_contents()->GetLastCommittedURL().spec();
    ReportDownloadStatusToRequestCoordinator(new_downloads_snapshot_info.get(),
                                             false);
    return;
  }

  // If there is an ongoing snapshot request, completely ignore this one and
  // cancel the Background Offliner request.
  // TODO(carlosk): it might be better to make the decision to schedule or not
  // the background request here. See https://crbug.com/686165.
  if (downloads_ongoing_snapshot_info_) {
    DVLOG(1) << "Ongoing request exist; ignored download request for: "
             << web_contents()->GetLastCommittedURL().spec();
    ReportDownloadStatusToRequestCoordinator(new_downloads_snapshot_info.get(),
                                             true);
    return;
  }

  // Stores the new snapshot info.
  downloads_ongoing_snapshot_info_ = std::move(new_downloads_snapshot_info);

  // If the page is not yet ready for a snapshot return now as it will be
  // started later, once page loading advances.
  if (PageQuality::POOR == snapshot_controller_->current_page_quality()) {
    DVLOG(1) << "Waiting for the page to load before fulfilling download "
             << "request for: " << web_contents()->GetLastCommittedURL().spec();
    downloads_snapshot_on_hold_ = true;
    return;
  }

  // Otherwise start saving the snapshot now.
  DVLOG(1) << "Starting download request for: "
           << web_contents()->GetLastCommittedURL().spec();
  SaveSnapshotForDownloads(false);
}

// Initialize lazily. It needs TabAndroid for initialization, which is also a
// TabHelper - so can't initialize in constructor because of uncertain order
// of creation of TabHelpers.
bool RecentTabHelper::EnsureInitialized() {
  if (snapshot_controller_)  // Initialized already.
    return snapshots_enabled_;

  snapshot_controller_ = std::make_unique<SnapshotController>(
      base::SingleThreadTaskRunner::GetCurrentDefault(), this);
  snapshot_controller_->Stop();  // It is reset when navigation commits.

  int tab_id_number = 0;
  tab_id_.clear();

  if (delegate_->GetTabId(web_contents(), &tab_id_number))
    tab_id_ = base::NumberToString(tab_id_number);

  // TODO(dimich): When we have BackgroundOffliner, avoid capturing prerenderer
  // WebContents with its origin as well.
  snapshots_enabled_ = !tab_id_.empty() &&
                       !web_contents()->GetBrowserContext()->IsOffTheRecord();

  if (snapshots_enabled_) {
    page_model_ = OfflinePageModelFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
  }

  return snapshots_enabled_;
}

void RecentTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    DVLOG_IF(1, navigation_handle->IsInPrimaryMainFrame())
        << "Primary main frame navigation ignored (reasons: "
        << !navigation_handle->HasCommitted() << ", "
        << navigation_handle->IsSameDocument()
        << ") to: " << web_contents()->GetLastCommittedURL().spec();
    return;
  }

  if (!EnsureInitialized())
    return;
  DVLOG(1) << "Navigation acknowledged to: "
           << web_contents()->GetLastCommittedURL().spec();

  // If there is an ongoing downloads request, lets make Background Offliner
  // take over downloading that page.
  if (downloads_ongoing_snapshot_info_) {
    DVLOG(1) << " - Passing ongoing downloads request to Background Offliner";
    ReportDownloadStatusToRequestCoordinator(
        downloads_ongoing_snapshot_info_.get(), false);
  }

  // If currently loading an offline page get a pointer to it. It will be null
  // otherwise.
  const OfflinePageItem* current_offline_page =
      OfflinePageUtils::GetOfflinePageFromWebContents(web_contents());

  // If the previous page was saved, delete it now unless we are currently
  // loading that very snapshot.
  if (last_n_latest_saved_snapshot_info_ &&
      (!current_offline_page ||
       current_offline_page->offline_id !=
           last_n_latest_saved_snapshot_info_->request_id)) {
    DVLOG(1) << " - Deleting previous last_n snapshot with offline_id "
             << last_n_latest_saved_snapshot_info_->request_id;
    PageCriteria criteria;
    criteria.offline_ids =
        std::vector<int64_t>{last_n_latest_saved_snapshot_info_->request_id};
    page_model_->DeletePagesWithCriteria(criteria, base::DoNothing());
    last_n_latest_saved_snapshot_info_.reset();
  }

  // Cancel any and all in flight snapshot tasks from the previous page.
  CancelInFlightSnapshots();
  downloads_snapshot_on_hold_ = false;

  // Always reset so that posted tasks get canceled.
  snapshot_controller_->Reset();

  // Check for conditions that should stop us from creating snapshots of
  // this page:
  // - It is an error page.
  // - The navigated URL is not supported.
  // - The page being loaded is already an offline page.
  bool can_save =
      !navigation_handle->IsErrorPage() &&
      OfflinePageModel::CanSaveURL(web_contents()->GetLastCommittedURL()) &&
      current_offline_page == nullptr;
  DVLOG_IF(1, !can_save)
      << " - Page can not be saved for offline usage (reasons: "
      << !navigation_handle->IsErrorPage() << ", "
      << OfflinePageModel::CanSaveURL(web_contents()->GetLastCommittedURL())
      << ", " << (current_offline_page == nullptr) << ")";

  if (!can_save)
    snapshot_controller_->Stop();
  // Last N should be disabled when:
  // - Running on low end devices.
  // - Viewing POST content for privacy considerations.
  // - Disabled by flag.
  last_n_listen_to_tab_hidden_ =
      can_save && !delegate_->IsLowEndDevice() && !navigation_handle->IsPost();
  DVLOG_IF(1, can_save && !last_n_listen_to_tab_hidden_)
      << " - Page can not be saved by last_n";
}

void RecentTabHelper::PrimaryMainDocumentElementAvailable() {
  EnsureInitialized();
  snapshot_controller_->PrimaryMainDocumentElementAvailable();
}

void RecentTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  EnsureInitialized();
  snapshot_controller_->DocumentOnLoadCompletedInPrimaryMainFrame();
}

void RecentTabHelper::WebContentsDestroyed() {
  // If there is an ongoing downloads request, lets allow Background Offliner to
  // continue downloading this page.
  if (downloads_ongoing_snapshot_info_) {
    DVLOG(1) << "WebContents destroyed; passing ongoing downloads request to "
                "Background Offliner";
    ReportDownloadStatusToRequestCoordinator(
        downloads_ongoing_snapshot_info_.get(), false);
  }
  // And cancel any ongoing snapshots.
  CancelInFlightSnapshots();
}

void RecentTabHelper::OnVisibilityChanged(content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    WebContentsWasHidden();
  else
    WebContentsWasShown();
}

void RecentTabHelper::WebContentsWasHidden() {
  // Do not save a snapshots if any of these are true:
  // - Last_n is not listening to tab hidden events.
  // - A last_n snapshot is currently being saved.
  // - The tab is in the process of being closed.
  // - The tab is currently presented as a custom tab.
  // Note that a WebContents may be embedded in another WebContents. The
  // outermost WebContents is the one associated with the tab.
  if (!last_n_listen_to_tab_hidden_ || last_n_ongoing_snapshot_info_ ||
      tab_is_closing_ ||
      delegate_->IsCustomTab(web_contents()->GetOutermostWebContents())) {
    DVLOG(1) << "Will not snapshot for last_n (reasons: "
             << !last_n_listen_to_tab_hidden_ << ", "
             << !!last_n_ongoing_snapshot_info_ << ", " << tab_is_closing_
             << ", "
             << delegate_->IsCustomTab(
                    web_contents()->GetOutermostWebContents())
             << ") for: " << web_contents()->GetLastCommittedURL().spec();
    return;
  }

  // Do not save if page quality is too low.
  // Note: we assume page quality for a page can only increase.
  if (snapshot_controller_->current_page_quality() == PageQuality::POOR) {
    DVLOG(1) << "Will not snapshot for last_n (page quality too low) for: "
             << web_contents()->GetLastCommittedURL().spec();
    return;
  }

  DVLOG(1) << "Starting last_n snapshot for: "
           << web_contents()->GetLastCommittedURL().spec();
  last_n_ongoing_snapshot_info_ =
      std::make_unique<SnapshotProgressInfo>(GetRecentPagesClientId());
  DCHECK(last_n_ongoing_snapshot_info_->IsForLastN());
  DCHECK(snapshots_enabled_);
  // Remove previously captured pages for this tab.
  page_model_->GetOfflineIdsForClientId(
      GetRecentPagesClientId(),
      base::BindOnce(&RecentTabHelper::ContinueSnapshotWithIdsToPurge,
                     weak_ptr_factory_.GetWeakPtr(),
                     last_n_ongoing_snapshot_info_.get()));

  last_n_latest_saved_snapshot_info_.reset();
}

void RecentTabHelper::WebContentsWasShown() {
  // If the tab was closing and is now being shown, the closure was reverted.
  DVLOG_IF(0, tab_is_closing_) << "Tab is not closing anymore: "
                               << web_contents()->GetLastCommittedURL().spec();
  tab_is_closing_ = false;
}

void RecentTabHelper::WillCloseTab() {
  DVLOG(1) << "Tab is now closing: "
           << web_contents()->GetLastCommittedURL().spec();
  tab_is_closing_ = true;
}

// TODO(carlosk): rename this to RequestSnapshot and make it return a bool
// representing the acceptance of the snapshot request.
void RecentTabHelper::StartSnapshot() {
  DCHECK_NE(PageQuality::POOR, snapshot_controller_->current_page_quality());

  // As long as snapshots are enabled for this tab, there are two situations
  // that allow for a navigation event to start a snapshot:
  // 1) There is a request on hold waiting for the page to be minimally loaded.
  if (snapshots_enabled_ && downloads_snapshot_on_hold_) {
    DVLOG(1) << "Resuming downloads snapshot request for: "
             << web_contents()->GetLastCommittedURL().spec();
    downloads_snapshot_on_hold_ = false;
    SaveSnapshotForDownloads(false);
    return;
  }

  // 2) There's no ongoing snapshot and a previous one was saved with lower
  // expected quality than what would be possible now.
  if (snapshots_enabled_ &&
      (!downloads_ongoing_snapshot_info_ &&
       downloads_latest_saved_snapshot_info_ &&
       downloads_latest_saved_snapshot_info_->expected_page_quality <
           snapshot_controller_->current_page_quality())) {
    DVLOG(1) << "Upgrading last downloads snapshot for: "
             << web_contents()->GetLastCommittedURL().spec();
    SaveSnapshotForDownloads(true);
    return;
  }

  // Notify the controller that a snapshot was not started.
  snapshot_controller_->PendingSnapshotCompleted();
}

void RecentTabHelper::SaveSnapshotForDownloads(bool replace_latest) {
  DCHECK_NE(PageQuality::POOR, snapshot_controller_->current_page_quality());

  if (replace_latest) {
    // Start by requesting the deletion of the existing previous snapshot of
    // this page.
    DCHECK(downloads_latest_saved_snapshot_info_);
    DCHECK(!downloads_ongoing_snapshot_info_);
    downloads_ongoing_snapshot_info_ = std::make_unique<SnapshotProgressInfo>(
        downloads_latest_saved_snapshot_info_->client_id,
        downloads_latest_saved_snapshot_info_->request_id,
        downloads_latest_saved_snapshot_info_->origin);
    std::vector<int64_t> ids{downloads_latest_saved_snapshot_info_->request_id};
    ContinueSnapshotWithIdsToPurge(downloads_ongoing_snapshot_info_.get(), ids);
  } else {
    // Otherwise go straight to saving the page.
    DCHECK(downloads_ongoing_snapshot_info_);
    ContinueSnapshotAfterPurge(downloads_ongoing_snapshot_info_.get(),
                               OfflinePageModel::DeletePageResult::SUCCESS);
  }
}

// This is the 1st step of a sequence of async operations chained through
// callbacks, mostly shared between last_n and downloads:
// 1) Compute the set of old 'last_n' pages that have to be purged.
// 2) Delete the pages found in the previous step.
// 3) Snapshot the current web contents.
// 4) Notify requesters about the final result of the operation.
//
// For last_n requests the sequence is always started in 1). For downloads it
// starts in either 2) or 3). Step 4) might be called anytime during the chain
// for early termination in case of errors.
void RecentTabHelper::ContinueSnapshotWithIdsToPurge(
    SnapshotProgressInfo* snapshot_info,
    const std::vector<int64_t>& page_ids) {
  DCHECK(snapshot_info);

  DVLOG_IF(1, !page_ids.empty()) << "Deleting " << page_ids.size()
                                 << " offline pages...";
  PageCriteria criteria;
  criteria.offline_ids = page_ids;
  page_model_->DeletePagesWithCriteria(
      criteria, base::BindOnce(&RecentTabHelper::ContinueSnapshotAfterPurge,
                               weak_ptr_factory_.GetWeakPtr(), snapshot_info));
}

void RecentTabHelper::ContinueSnapshotAfterPurge(
    SnapshotProgressInfo* snapshot_info,
    OfflinePageModel::DeletePageResult result) {
  if (result != OfflinePageModel::DeletePageResult::SUCCESS) {
    ReportSnapshotCompleted(snapshot_info, false);
    return;
  }

  DCHECK(OfflinePageModel::CanSaveURL(web_contents()->GetLastCommittedURL()));
  snapshot_info->expected_page_quality =
      snapshot_controller_->current_page_quality();
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = web_contents()->GetLastCommittedURL();
  save_page_params.client_id = snapshot_info->client_id;
  save_page_params.proposed_offline_id = snapshot_info->request_id;
  save_page_params.is_background = false;
  save_page_params.original_url =
      OfflinePageUtils::GetOriginalURLFromWebContents(web_contents());
  save_page_params.request_origin = snapshot_info->origin;
  page_model_->SavePage(
      save_page_params, delegate_->CreatePageArchiver(web_contents()),
      web_contents(),
      base::BindOnce(&RecentTabHelper::SavePageCallback,
                     weak_ptr_factory_.GetWeakPtr(), snapshot_info));
}

void RecentTabHelper::SavePageCallback(SnapshotProgressInfo* snapshot_info,
                                       OfflinePageModel::SavePageResult result,
                                       int64_t offline_id) {
  DCHECK((snapshot_info->IsForLastN() &&
          snapshot_info->request_id == OfflinePageModel::kInvalidOfflineId) ||
         result != SavePageResult::SUCCESS ||
         snapshot_info->request_id == offline_id)
      << "SnapshotProgressInfo(client_id=" << snapshot_info->client_id
      << ", request_id=" << snapshot_info->request_id
      << ", origin=" << snapshot_info->origin << "), offline_id=" << offline_id;
  // Store the assigned offline_id (for downloads case it will already contain
  // the same value).
  snapshot_info->request_id = offline_id;
  ReportSnapshotCompleted(snapshot_info, result == SavePageResult::SUCCESS);
}

// Note: this is the final step in the chain of callbacks and it's where the
// behavior is different depending on this being a last_n or downloads snapshot.
void RecentTabHelper::ReportSnapshotCompleted(
    SnapshotProgressInfo* snapshot_info,
    bool success) {
  DVLOG(1) << (snapshot_info->IsForLastN() ? "Last_n" : "Downloads")
           << " snapshot " << (success ? "succeeded" : "failed")
           << " for: " << web_contents()->GetLastCommittedURL().spec();
  if (snapshot_info->IsForLastN()) {
    DCHECK_EQ(snapshot_info, last_n_ongoing_snapshot_info_.get());
    if (success) {
      last_n_latest_saved_snapshot_info_ =
          std::move(last_n_ongoing_snapshot_info_);
    } else {
      last_n_ongoing_snapshot_info_.reset();
    }
    return;
  }

  DCHECK_EQ(snapshot_info, downloads_ongoing_snapshot_info_.get());
  snapshot_controller_->PendingSnapshotCompleted();
  // Tell RequestCoordinator how the request should be processed further.
  ReportDownloadStatusToRequestCoordinator(snapshot_info, success);
  if (success) {
    downloads_latest_saved_snapshot_info_ =
        std::move(downloads_ongoing_snapshot_info_);
  } else {
    downloads_ongoing_snapshot_info_.reset();
  }
}

// Note: we cannot assume that snapshot_info == downloads_latest_snapshot_info_
// because further calls made to ObserveAndDownloadCurrentPage will replace
// downloads_latest_snapshot_info_ with a new instance.
void RecentTabHelper::ReportDownloadStatusToRequestCoordinator(
    SnapshotProgressInfo* snapshot_info,
    bool cancel_background_request) {
  DCHECK(snapshot_info);
  DCHECK(!snapshot_info->IsForLastN());

  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (!request_coordinator)
    return;

  // It is OK to call these methods more then once, depending on
  // number of snapshots attempted in this tab helper. If the request_id is not
  // in the list of RequestCoordinator, these calls have no effect.
  if (cancel_background_request) {
    request_coordinator->MarkRequestCompleted(snapshot_info->request_id);
  } else {
    request_coordinator->EnableForOffliner(snapshot_info->request_id,
                                           snapshot_info->client_id);
  }
}

ClientId RecentTabHelper::GetRecentPagesClientId() const {
  return ClientId(kLastNNamespace, tab_id_);
}

void RecentTabHelper::CancelInFlightSnapshots() {
  DVLOG_IF(1, last_n_ongoing_snapshot_info_)
      << " - Canceling ongoing last_n snapshot";
  DVLOG_IF(1, downloads_ongoing_snapshot_info_)
      << " - Canceling ongoing downloads snapshot";
  weak_ptr_factory_.InvalidateWeakPtrs();
  downloads_ongoing_snapshot_info_.reset();
  downloads_latest_saved_snapshot_info_.reset();
  last_n_ongoing_snapshot_info_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(RecentTabHelper);

}  // namespace offline_pages
