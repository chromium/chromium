// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_auto_fetcher.h"

#include "base/time/time.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

constexpr int kMaximumInFlight = 3;

ClientId URLToClientId(const GURL& url) {
  // By using namespace.URL as the client ID, we ensure that only a single
  // request for a page can be in flight.

  // We strip the fragment, because when loading an offline page, only the most
  // recent page saved with the URL (fragment stripped) can be loaded.
  GURL::Replacements remove_fragment;
  remove_fragment.ClearRef();
  GURL url_to_match = url.ReplaceComponents(remove_fragment);

  return ClientId(kAutoAsyncNamespace, url_to_match.spec());
}

}  // namespace

// This is an attempt to verify that a task callback eventually calls
// TaskComplete exactly once. If the token is never std::move'd, it will DCHECK
// when it is destroyed.
class OfflinePageAutoFetcher::TaskToken {
 public:
  // The static methods should only be called by StartOrEnqueue or TaskComplete.
  static TaskToken NewToken() { return TaskToken(); }
  static void Finalize(TaskToken& token) { token.alive_ = false; }

  TaskToken(TaskToken&& other) : alive_(other.alive_) {
    DCHECK(other.alive_);
    other.alive_ = false;
  }
  ~TaskToken() { DCHECK(!alive_); }

 private:
  TaskToken() {}

  bool alive_ = true;
  DISALLOW_COPY_AND_ASSIGN(TaskToken);
};

OfflinePageAutoFetcher::OfflinePageAutoFetcher(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

OfflinePageAutoFetcher::~OfflinePageAutoFetcher() = default;

void OfflinePageAutoFetcher::TrySchedule(bool user_requested,
                                         const GURL& url,
                                         TryScheduleCallback callback) {
  StartOrEnqueue(base::BindOnce(&OfflinePageAutoFetcher::TryScheduleStep1,
                                GetWeakPtr(), user_requested, url,
                                std::move(callback)));
}

void OfflinePageAutoFetcher::CancelSchedule(const GURL& url) {
  StartOrEnqueue(base::BindOnce(&OfflinePageAutoFetcher::CancelScheduleStep1,
                                GetWeakPtr(), url));
}

void OfflinePageAutoFetcher::TryScheduleStep1(bool user_requested,
                                              const GURL& url,
                                              TryScheduleCallback callback,
                                              TaskToken token) {
  // Return an early failure if the URL is not suitable.
  if (!OfflinePageModel::CanSaveURL(url)) {
    std::move(callback).Run(OfflinePageAutoFetcherScheduleResult::kOtherError);
    TaskComplete(std::move(token));
    return;
  }

  // We need to do some checks on in-flight requests before scheduling the
  // fetch. So first, get the list of all requests, and proceed to step 2.
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context_);

  coordinator->GetAllRequests(
      base::BindOnce(&OfflinePageAutoFetcher::TryScheduleStep2, GetWeakPtr(),
                     std::move(token), user_requested, url, std::move(callback),
                     // Unretained is OK because coordinator is calling us back.
                     base::Unretained(coordinator)));
}

void OfflinePageAutoFetcher::TryScheduleStep2(
    TaskToken token,
    bool user_requested,
    const GURL& url,
    TryScheduleCallback callback,
    RequestCoordinator* coordinator,
    std::vector<std::unique_ptr<SavePageRequest>> all_requests) {
  // If a request for this page is already scheduled, report scheduling as
  // successful without doing anything.
  const ClientId url_client_id = URLToClientId(url);
  for (const auto& request : all_requests) {
    if (url_client_id == request->client_id()) {
      std::move(callback).Run(
          OfflinePageAutoFetcherScheduleResult::kAlreadyScheduled);
      TaskComplete(std::move(token));
      return;
    }
  }

  // Respect kMaximumInFlight.
  if (!user_requested) {
    int in_flight_count = 0;
    for (const auto& request : all_requests) {
      if (request->client_id().name_space == kAutoAsyncNamespace) {
        ++in_flight_count;
      }
    }
    if (in_flight_count >= kMaximumInFlight) {
      std::move(callback).Run(
          OfflinePageAutoFetcherScheduleResult::kNotEnoughQuota);
      TaskComplete(std::move(token));
      return;
    }
  }

  // Finally, schedule a new request, and proceed to step 3.
  RequestCoordinator::SavePageLaterParams params;
  params.url = url;
  params.client_id = url_client_id;
  params.user_requested = false;
  params.availability =
      RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER;
  coordinator->SavePageLater(
      params,
      base::BindOnce(&OfflinePageAutoFetcher::TryScheduleStep3, GetWeakPtr(),
                     std::move(token), std::move(callback)));
}

void OfflinePageAutoFetcher::TryScheduleStep3(TaskToken token,
                                              TryScheduleCallback callback,
                                              AddRequestResult result) {
  // Just forward the response to the mojo caller.
  std::move(callback).Run(
      result == AddRequestResult::SUCCESS
          ? OfflinePageAutoFetcherScheduleResult::kScheduled
          : OfflinePageAutoFetcherScheduleResult::kOtherError);
  TaskComplete(std::move(token));
}

void OfflinePageAutoFetcher::CancelScheduleStep1(const GURL& url,
                                                 TaskToken token) {
  // Get all requests, and proceed to step 2.
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context_);
  coordinator->GetAllRequests(
      base::BindOnce(&OfflinePageAutoFetcher::CancelScheduleStep2, GetWeakPtr(),
                     std::move(token), url, coordinator));
}

void OfflinePageAutoFetcher::CancelScheduleStep2(
    TaskToken token,
    const GURL& url,
    RequestCoordinator* coordinator,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  // Cancel the request if it's found in the list of all requests.
  const ClientId url_client_id = URLToClientId(url);
  for (const auto& request : requests) {
    if (url_client_id == request->client_id()) {
      coordinator->RemoveRequests(
          {request->request_id()},
          base::BindOnce(&OfflinePageAutoFetcher::CancelScheduleStep3,
                         GetWeakPtr(), std::move(token)));
      return;
    }
  }
  TaskComplete(std::move(token));
}

void OfflinePageAutoFetcher::CancelScheduleStep3(TaskToken token,
                                                 const MultipleItemStatuses&) {
  TaskComplete(std::move(token));
}

void OfflinePageAutoFetcher::StartOrEnqueue(TaskCallback task) {
  bool can_run = task_queue_.empty();
  task_queue_.push(std::move(task));
  if (can_run)
    std::move(task_queue_.front()).Run(TaskToken::NewToken());
}

void OfflinePageAutoFetcher::TaskComplete(TaskToken token) {
  TaskToken::Finalize(token);
  DCHECK(!task_queue_.empty());
  DCHECK(!task_queue_.front());
  task_queue_.pop();
  if (!task_queue_.empty())
    std::move(task_queue_.front()).Run(TaskToken::NewToken());
}

bool OfflinePageAutoFetcher::IsTaskQueueEmptyForTesting() {
  return task_queue_.empty();
}

}  // namespace offline_pages
