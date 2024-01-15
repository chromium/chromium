// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_AUTO_FETCH_PAGE_LOAD_WATCHER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_AUTO_FETCH_PAGE_LOAD_WATCHER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/auto_fetch.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class NavigationHandle;
}  // namespace content

namespace offline_pages {
class SavePageRequest;
class RequestCoordinator;

// Manages showing the in-progress notification.
class AutoFetchNotifier {
 public:
  virtual ~AutoFetchNotifier() {}
  // Ensures that the in-progress notification is showing with the appropriate
  // request count.
  virtual void NotifyInProgress(int in_flight_count) {}
  // Update the request count if the in-progress notification is already
  // showing. This won't trigger showing the notification if it's not already
  // shown. If |in_flight_count| is 0, the notification will be hidden.
  virtual void InProgressCountChanged(int in_flight_count) {}
};

// Types and functions internal to AutoFetchPageLoadWatcher. Included in the
// header for testing.
namespace auto_fetch_internal {

// Information about an Android browser tab.
struct TabInfo {
  int android_tab_id = 0;
  GURL current_url;
};

// Interface to Android tabs used by |AutoFetchPageLoadWatcher|. This is the
// real implementation, methods are virtual for testing only.
class AndroidTabFinder {
 public:
  virtual ~AndroidTabFinder();
  // Returns a mapping of Android tab ID to TabInfo.
  virtual std::map<int, TabInfo> FindAndroidTabs(
      std::vector<int> android_tab_ids);
  virtual std::optional<TabInfo> FindNavigationTab(
      content::WebContents* web_contents);
};

// Information about an auto-fetch |SavePageRequest|.
struct RequestInfo {
  int64_t request_id;
  GURL url;
  SavePageRequest::AutoFetchNotificationState notification_state;
  auto_fetch::ClientIdMetadata metadata;
};

std::optional<RequestInfo> MakeRequestInfo(const SavePageRequest& request);

// |AutoFetchPageLoadWatcher|'s more unit-testable internal implementation.
// This class was designed to have few dependencies to make testing more
// tractable. Events are communicated to |InternalImpl| through its public
// member functions, and functions are injected through |AutoFetchNotifier|,
// |Delegate|, and |AndroidTabFinder|.
class InternalImpl {
 public:
  // Injected functions, implemented by |AutoFetchPageLoadWatcher|.
  // We need this because we can't call these functions directly.
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Sets the notification state of a request to
    // |SavePageRequest::AutoFetchNotificationState::kShown|. Results in a call
    // to |SetNotificationStateComplete|.
    virtual void SetNotificationStateToShown(int64_t request_id) {}
    // Removes all |SavePageRequest|s with the given IDs.
    virtual void RemoveRequests(const std::vector<int64_t>& request_ids) {}
  };

  InternalImpl(AutoFetchNotifier* notifier,
               Delegate* delegate,
               std::unique_ptr<AndroidTabFinder> tab_finder);
  ~InternalImpl();

  // Request state change methods.
  void RequestListInitialized(std::vector<RequestInfo> requests);
  void RequestAdded(RequestInfo info);
  void RequestRemoved(RequestInfo info);
  void SetNotificationStateComplete(int64_t request_id, bool success);

  // Navigation methods.
  void SuccessfulPageNavigation(const GURL& url);
  void NavigationFrom(const GURL& previous_url,
                      content::WebContents* web_contents);

  // Tab event methods.
  void TabClosed(int android_tab_id);
  void TabModelReady();

  base::WeakPtr<InternalImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  std::vector<GURL>& pages_loaded_before_observer_ready_for_testing() {
    return pages_loaded_before_observer_ready_;
  }

 private:
  void SetNotificationStateToShown(int64_t request_id);
  void UpdateNotificationStateForAllRequests();

  raw_ptr<AutoFetchNotifier> notifier_;
  raw_ptr<Delegate> delegate_;
  std::unique_ptr<AndroidTabFinder> tab_finder_;
  std::vector<RequestInfo> requests_;
  // Tracks whether |RequestListInitialized| has been called. If false,
  // |RequestAdded| and |RequestRemoved| should be ignored, as per the
  // documentation in |RequestCoordinator::Observer|.
  bool requests_initialized_ = false;
  bool tab_model_ready_ = false;
  std::vector<GURL> pages_loaded_before_observer_ready_;

  base::WeakPtrFactory<InternalImpl> weak_ptr_factory_{this};
};

}  // namespace auto_fetch_internal

// Watches for page loads and |RequestCoordinator| requests.
// Given an active auto-fetch request for <tab, URL>:
// - If URL is loaded successfully on tab, cancel the auto-fetch request.
// - If a different URL is loaded successfully on tab, trigger the in-progress
//   notification.
// - If tab is closed, trigger the in-progress notification.
// - If an auto-fetch request is removed, update the in-progress notification's
//   displayed request count.
//
// Implementation note:
// This class simply observes events and passes them down to |InternalImpl|
// for processing. All code here is run on the UI thread.
class AutoFetchPageLoadWatcher
    : public RequestCoordinator::Observer,
      public auto_fetch_internal::InternalImpl::Delegate {
 public:
  using AndroidTabFinder = auto_fetch_internal::AndroidTabFinder;

  static void CreateForWebContents(content::WebContents* web_contents);

  AutoFetchPageLoadWatcher(AutoFetchNotifier* notifier,
                           RequestCoordinator* request_coordinator,
                           std::unique_ptr<AndroidTabFinder> tab_finder);

  AutoFetchPageLoadWatcher(const AutoFetchPageLoadWatcher&) = delete;
  AutoFetchPageLoadWatcher& operator=(const AutoFetchPageLoadWatcher&) = delete;

  ~AutoFetchPageLoadWatcher() override;

  // Called when navigation completes, even on errors. This is only called
  // once per navigation.
  void HandleNavigation(content::NavigationHandle* navigation_handle);

  std::vector<GURL>& loaded_pages_for_testing() {
    return impl_.pages_loaded_before_observer_ready_for_testing();
  }

 private:
  class NavigationObserver;
  class TabWatcher;
  base::WeakPtr<AutoFetchPageLoadWatcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void InitializeRequestList(
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  // InternalImpl::Delegate.
  void SetNotificationStateToShown(int64_t request_id) override;
  void RemoveRequests(const std::vector<int64_t>& request_ids) override;

  // RequestCoordinator::Observer.
  void OnAdded(const SavePageRequest& request) override;
  void OnCompleted(const SavePageRequest& request,
                   RequestNotifier::BackgroundSavePageResult status) override;
  void OnChanged(const SavePageRequest& request) override {}
  void OnNetworkProgress(const SavePageRequest& request,
                         int64_t received_bytes) override {}

  raw_ptr<RequestCoordinator> request_coordinator_;  // Not owned.
  auto_fetch_internal::InternalImpl impl_;
  std::unique_ptr<TabWatcher> tab_watcher_;
  base::WeakPtrFactory<AutoFetchPageLoadWatcher> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_AUTO_FETCH_PAGE_LOAD_WATCHER_H_
