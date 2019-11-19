// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/offline_pages/android/auto_fetch_notifier.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/offline_pages/core/auto_fetch.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_item_utils.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {
constexpr int kMaximumInFlight = 3;

class AutoFetchNotifierImpl : public AutoFetchNotifier {
 public:
  ~AutoFetchNotifierImpl() override {}
  // Ensures that the in-progress notification is showing with the appropriate
  // request count.
  void NotifyInProgress(int in_flight_count) override {
    ShowAutoFetchInProgressNotification(in_flight_count);
  }
  // Update the request count if the in-progress notification is already
  // showing. This won't trigger showing the notification if it's not already
  // shown. If |in_flight_count| is 0, the notification will be hidden.
  void InProgressCountChanged(int in_flight_count) override {
    UpdateAutoFetchInProgressNotificationCountIfShowing(in_flight_count);
  }
};

}  // namespace

OfflinePageAutoFetcherService::OfflinePageAutoFetcherService(
    RequestCoordinator* request_coordinator,
    OfflinePageModel* offline_page_model,
    Delegate* delegate)
    : notifier_(std::make_unique<AutoFetchNotifierImpl>()),
      page_load_watcher_(
          notifier_.get(),
          request_coordinator,
          std::make_unique<AutoFetchPageLoadWatcher::AndroidTabFinder>()),
      request_coordinator_(request_coordinator),
      offline_page_model_(offline_page_model),
      delegate_(delegate) {
  request_coordinator_->AddObserver(this);
  if (AutoFetchInProgressNotificationCanceled()) {
    CancelAll(base::BindOnce(&AutoFetchCancellationComplete));
  }
}

OfflinePageAutoFetcherService::~OfflinePageAutoFetcherService() = default;

void OfflinePageAutoFetcherService::Shutdown() {
  request_coordinator_->RemoveObserver(this);
}

void OfflinePageAutoFetcherService::TrySchedule(bool user_requested,
                                                const GURL& url,
                                                int android_tab_id,
                                                TryScheduleCallback callback) {
  // Return an early failure if the URL is not suitable.
  if (!OfflinePageModel::CanSaveURL(url)) {
    std::move(callback).Run(OfflinePageAutoFetcherScheduleResult::kOtherError);
    return;
  }

  // Attempt to schedule a new request.
  RequestCoordinator::SavePageLaterParams params;
  params.url = url;
  auto_fetch::ClientIdMetadata metadata(android_tab_id);
  params.client_id = auto_fetch::MakeClientId(metadata);
  params.user_requested = false;
  params.availability =
      RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER;

  params.add_options.disallow_duplicate_requests = true;
  if (!user_requested) {
    params.add_options.maximum_in_flight_requests_for_namespace =
        kMaximumInFlight;
  }
  request_coordinator_->SavePageLater(
      params, base::BindOnce(&OfflinePageAutoFetcherService::TryScheduleDone,
                             GetWeakPtr(), std::move(callback)));
}

void OfflinePageAutoFetcherService::CancelAll(base::OnceClosure callback) {
  auto condition = base::BindRepeating([](const SavePageRequest& request) {
    return request.client_id().name_space == kAutoAsyncNamespace;
  });

  request_coordinator_->RemoveRequestsIf(
      condition, base::BindOnce(&OfflinePageAutoFetcherService::CancelAllDone,
                                GetWeakPtr(), std::move(callback)));
}

void OfflinePageAutoFetcherService::CancelSchedule(const GURL& url) {
  auto predicate = base::BindRepeating(
      [](const GURL& url, const SavePageRequest& request) {
        return request.client_id().name_space == kAutoAsyncNamespace &&
               EqualsIgnoringFragment(request.url(), url);
      },
      url);
  request_coordinator_->RemoveRequestsIf(predicate,
                                         /*done_callback=*/base::DoNothing());
}

void OfflinePageAutoFetcherService::CancelAllDone(
    base::OnceClosure callback,
    const MultipleItemStatuses& result) {
  std::move(callback).Run();
}

void OfflinePageAutoFetcherService::TryScheduleDone(
    TryScheduleCallback callback,
    AddRequestResult result) {
  // Translate the result and forward to the mojo caller.
  OfflinePageAutoFetcherScheduleResult callback_result;
  switch (result) {
    case AddRequestResult::REQUEST_QUOTA_HIT:
      callback_result = OfflinePageAutoFetcherScheduleResult::kNotEnoughQuota;
      break;
    case AddRequestResult::DUPLICATE_URL:
    case AddRequestResult::ALREADY_EXISTS:
      callback_result = OfflinePageAutoFetcherScheduleResult::kAlreadyScheduled;
      break;
    case AddRequestResult::SUCCESS:
      callback_result = OfflinePageAutoFetcherScheduleResult::kScheduled;
      break;
    case AddRequestResult::STORE_FAILURE:
    case AddRequestResult::URL_ERROR:
      callback_result = OfflinePageAutoFetcherScheduleResult::kOtherError;
      break;
  }
  std::move(callback).Run(callback_result);
}

void OfflinePageAutoFetcherService::OnCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  if (request.client_id().name_space != kAutoAsyncNamespace ||
      status != RequestNotifier::BackgroundSavePageResult::SUCCESS)
    return;

  offline_page_model_->GetPageByOfflineId(
      request.request_id(),
      base::BindOnce(&OfflinePageAutoFetcherService::AutoFetchComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageAutoFetcherService::AutoFetchComplete(
    const OfflinePageItem* page) {
  if (!page)
    return;
  base::Optional<auto_fetch::ClientIdMetadata> metadata =
      auto_fetch::ExtractMetadata(page->client_id);
  if (!metadata)
    return;

  delegate_->ShowAutoFetchCompleteNotification(
      page->title, page->GetOriginalUrl().spec(), page->url.spec(),
      metadata.value().android_tab_id, page->offline_id);
}

}  // namespace offline_pages
