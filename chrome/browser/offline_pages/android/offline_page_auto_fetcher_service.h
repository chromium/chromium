// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_H_

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/offline_pages/android/auto_fetch_page_load_watcher.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "url/gurl.h"

namespace offline_pages {
class RequestCoordinator;
class SavePageRequest;

// Implementation notes for the auto-fetch-on-net-error-page feature:
//
// The 'auto-fetch-on-net-error-page' (auto-fetch) feature kicks in when a
// 'dino' (offline error) page is reached. Chrome will schedule a request to
// save the page when the device gains connectivity. Users can cancel or
// explicitly request this behavior through UI on the dino page. Chrome attempts
// to avoid doing a background page save if the user ends up successfully
// navigating to the page. If the page is saved in the background, the a system
// notification is presented.
//
// Background page saves are implemented through |RequestCoordinator|. The
// |OfflinePageClientPolicy| for this feature is configured with the option
// |defer_background_fetch_while_page_is_active|. This instructs
// |RequestCoordinator| to first check if the page to be saved is currently
// active. If it is, the request is deferred. If a request is deferred 5 times,
// it is considered failed and removed. For this feature, we expect this
// condition to be rare because |RequestCoordinator| only processes requests
// when the device is connected, and the dino page automatically reloads when
// the device is connected.
//
// Additionally, save page requests are removed upon successful navigation
// commit. See |AutoFetchPageLoadWatcher|.

// A KeyedService for the auto-fetch feature.
// * Provides an interface to schedule and cancel auto-fetch requests.
// * Listens for complete fetches with RequestCoordinator::Observer, and
//   triggers the system notification.
class OfflinePageAutoFetcherService : public KeyedService,
                                      public RequestCoordinator::Observer {
 public:
  using OfflinePageAutoFetcherScheduleResult =
      chrome::mojom::OfflinePageAutoFetcherScheduleResult;
  using TryScheduleCallback = base::OnceCallback<void(
      chrome::mojom::OfflinePageAutoFetcherScheduleResult)>;

  // Injected interface for testing.
  class Delegate {
   public:
    // Calls |offline_pages::ShowAutoFetchCompleteNotification()|.
    virtual void ShowAutoFetchCompleteNotification(
        const base::string16& pageTitle,
        const std::string& original_url,
        const std::string& final_url,
        int android_tab_id,
        int64_t offline_id) = 0;
  };

  explicit OfflinePageAutoFetcherService(
      RequestCoordinator* request_coordinator,
      OfflinePageModel* offline_page_model,
      Delegate* delegate);
  ~OfflinePageAutoFetcherService() override;

  AutoFetchPageLoadWatcher* page_load_watcher() { return &page_load_watcher_; }

  // Auto fetching interface. Schedules and cancels fetch requests.
  void TrySchedule(bool user_requested,
                   const GURL& url,
                   int android_tab_id,
                   TryScheduleCallback callback);
  void CancelSchedule(const GURL& url);
  void CancelAll(base::OnceClosure callback);

  // KeyedService implementation.
  void Shutdown() override;

  // Testing methods.
  bool IsTaskQueueEmptyForTesting();

  // RequestCoordinator::Observer implementation.
  void OnAdded(const SavePageRequest& request) override {}
  void OnCompleted(const SavePageRequest& request,
                   RequestNotifier::BackgroundSavePageResult status) override;
  void OnChanged(const SavePageRequest& request) override {}
  void OnNetworkProgress(const SavePageRequest& request,
                         int64_t received_bytes) override {}

 private:
  base::WeakPtr<OfflinePageAutoFetcherService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void TryScheduleDone(TryScheduleCallback callback, AddRequestResult result);
  void AutoFetchComplete(const OfflinePageItem* page);
  void CancelAllDone(base::OnceClosure callback,
                     const MultipleItemStatuses& result);

  std::unique_ptr<AutoFetchNotifier> notifier_;
  AutoFetchPageLoadWatcher page_load_watcher_;
  RequestCoordinator* request_coordinator_;
  OfflinePageModel* offline_page_model_;
  Delegate* delegate_;
  base::WeakPtrFactory<OfflinePageAutoFetcherService> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_H_
