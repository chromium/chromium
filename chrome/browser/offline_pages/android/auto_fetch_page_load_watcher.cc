// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/auto_fetch_page_load_watcher.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "components/offline_pages/core/auto_fetch.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace offline_pages {
using auto_fetch_internal::AndroidTabFinder;
using auto_fetch_internal::InternalImpl;
using auto_fetch_internal::MakeRequestInfo;
using auto_fetch_internal::RequestInfo;
using auto_fetch_internal::TabInfo;

AndroidTabFinder::~AndroidTabFinder() = default;

TabInfo AnroidTabInfo(const TabAndroid& tab) {
  return {tab.GetAndroidId(), tab.GetURL()};
}

std::map<int, TabInfo> AndroidTabFinder::FindAndroidTabs(
    std::vector<int> android_tab_ids) {
  std::map<int, TabInfo> result;
  if (android_tab_ids.empty())
    return result;

  for (const TabModel* model : TabModelList::models()) {
    if (model->IsOffTheRecord())
      continue;

    for (int index = 0; index < model->GetTabCount(); ++index) {
      TabAndroid* tab = model->GetTabAt(index);
      if (base::Contains(android_tab_ids, tab->GetAndroidId())) {
        result[tab->GetAndroidId()] = AnroidTabInfo(*tab);
      }
    }
  }
  return result;
}

std::optional<TabInfo> AndroidTabFinder::FindNavigationTab(
    content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab)
    return std::nullopt;
  return AnroidTabInfo(*tab);
}

// Observes a WebContents to relay navigation events to
// AutoFetchPageLoadWatcher.
class AutoFetchPageLoadWatcher::NavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          AutoFetchPageLoadWatcher::NavigationObserver> {
 public:
  explicit NavigationObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        content::WebContentsUserData<
            AutoFetchPageLoadWatcher::NavigationObserver>(*web_contents) {
    page_load_watcher_ =
        OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(
            web_contents->GetBrowserContext())
            ->page_load_watcher();
    DCHECK(page_load_watcher_);
  }

  NavigationObserver(const NavigationObserver&) = delete;
  NavigationObserver& operator=(const NavigationObserver&) = delete;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasCommitted())
      return;
    page_load_watcher_->HandleNavigation(navigation_handle);
  }

 private:
  friend class content::WebContentsUserData<
      AutoFetchPageLoadWatcher::NavigationObserver>;
  raw_ptr<AutoFetchPageLoadWatcher> page_load_watcher_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutoFetchPageLoadWatcher::NavigationObserver);

// static
void AutoFetchPageLoadWatcher::CreateForWebContents(
    content::WebContents* web_contents) {
  OfflinePageAutoFetcherService* service =
      OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  // Don't try to create if the service isn't available (happens in incognito
  // mode).
  if (service) {
    NavigationObserver::CreateForWebContents(web_contents);
  }
}

namespace auto_fetch_internal {

std::optional<RequestInfo> MakeRequestInfo(const SavePageRequest& request) {
  std::optional<auto_fetch::ClientIdMetadata> metadata =
      auto_fetch::ExtractMetadata(request.client_id());
  if (!metadata)
    return std::nullopt;

  RequestInfo info;
  info.request_id = request.request_id();
  info.url = request.url();
  info.metadata = metadata.value();
  info.notification_state = request.auto_fetch_notification_state();
  return info;
}

InternalImpl::InternalImpl(AutoFetchNotifier* notifier,
                           Delegate* delegate,
                           std::unique_ptr<AndroidTabFinder> tab_finder)
    : notifier_(notifier),
      delegate_(delegate),
      tab_finder_(std::move(tab_finder)) {}

InternalImpl::~InternalImpl() {}

void InternalImpl::RequestListInitialized(std::vector<RequestInfo> request) {
  DCHECK(!requests_initialized_);
  requests_initialized_ = true;
  requests_ = std::move(request);

  for (const GURL& url : pages_loaded_before_observer_ready_) {
    SuccessfulPageNavigation(url);
  }
  pages_loaded_before_observer_ready_.clear();

  if (tab_model_ready_)
    UpdateNotificationStateForAllRequests();
}

void InternalImpl::UpdateNotificationStateForAllRequests() {
  DCHECK(requests_initialized_);
  DCHECK(tab_model_ready_);
  // Now that we have the full list of requests, we need to verify that the
  // notification state is correct. For instance, if a tab was closed or
  // naviagated away from the request URL, we need to trigger the in-progress
  // notification.

  // For requests that haven't yet produced an in-progress notification, we need
  // to find out if the request URL is currently bound to the expected tab. If
  // not, trigger the in-progress notification.
  std::vector<int> android_tab_ids;
  for (const RequestInfo& request : requests_) {
    if (request.notification_state ==
        SavePageRequest::AutoFetchNotificationState::kUnknown) {
      android_tab_ids.push_back(request.metadata.android_tab_id);
    }
  }

  const std::map<int, TabInfo> android_tabs =
      tab_finder_->FindAndroidTabs(android_tab_ids);
  for (RequestInfo& request : requests_) {
    if (request.notification_state ==
        SavePageRequest::AutoFetchNotificationState::kUnknown) {
      auto tab_iterator = android_tabs.find(request.metadata.android_tab_id);
      if (tab_iterator == android_tabs.end() ||
          tab_iterator->second.current_url != request.url) {
        SetNotificationStateToShown(request.request_id);
      }
    }
  }
}

void InternalImpl::RequestAdded(RequestInfo request) {
  if (!requests_initialized_)
    return;

  requests_.push_back(request);
  // Because interaction with RequestCoordinator is asynchronous, we need to
  // check if the request is no longer tied to a tab, and issue the in-progress
  // notification.
  if (request.notification_state ==
      SavePageRequest::AutoFetchNotificationState::kShown)
    return;

  // If the tab model isn't ready yet, don't do anything yet. Everything will be
  // reconciled in |UpdateNotificationStateForAllRequests()| later.
  if (!tab_model_ready_)
    return;

  const std::map<int, TabInfo> android_tabs =
      tab_finder_->FindAndroidTabs({request.metadata.android_tab_id});
  if (android_tabs.empty())
    delegate_->SetNotificationStateToShown(request.request_id);

  // TODO(harringtond): it's also possible that the request should be removed
  // because a successful navigation happened before the request could be added
  // to the database. We might be able to catch this case by remembering some
  // set of previous successful navigations along with timestamps, but even that
  // isn't perfect.
  // The upshot is that we risk auto-fetching a page and notifying the user even
  // after they've already loaded it.
}

void InternalImpl::RequestRemoved(RequestInfo request) {
  if (!requests_initialized_)
    return;

  for (size_t i = 0; i < requests_.size(); ++i) {
    RequestInfo info = requests_[i];
    if (info.request_id == request.request_id)
      requests_.erase(requests_.begin() + i);
  }
  notifier_->InProgressCountChanged(requests_.size());
}

void InternalImpl::SetNotificationStateComplete(int64_t request_id,
                                                bool success) {
  if (!success)
    return;

  notifier_->NotifyInProgress(requests_.size());
}

// Called when a successful navigation to |url| happens.
// If URL is loaded successfully on tab, cancel the auto-fetch request.
void InternalImpl::SuccessfulPageNavigation(const GURL& url) {
  // Early exit for the common-case.
  if (requests_initialized_ && requests_.empty())
    return;

  // If the request list isn't yet initialized, we have to defer handling of the
  // event. Never accumulate more than a few, so we can't have a boundless
  // array. This means we will fail to cancel an auto-fetch request if too many
  // navigations occur before |RequestListInitialized|.
  if (!requests_initialized_) {
    if (pages_loaded_before_observer_ready_.size() < 10)
      pages_loaded_before_observer_ready_.push_back(url);
    return;
  }

  std::vector<int64_t> remove_ids;
  for (const RequestInfo& request : requests_) {
    if (request.url == url)
      remove_ids.push_back(request.request_id);
  }
  if (!remove_ids.empty())
    delegate_->RemoveRequests(remove_ids);
}

void InternalImpl::NavigationFrom(const GURL& previous_url,
                                  content::WebContents* web_contents) {
  // Early exit for the common-case. We can ignore events from before the
  // request list is initialized because we reconcile things in
  // |RequestListInitialized|.
  if (!requests_initialized_ || requests_.empty())
    return;

  // Find requests that haven't yet been notified, and that match the
  // navigated-from URL.
  for (RequestInfo& request : requests_) {
    if (request.url == previous_url &&
        request.notification_state ==
            SavePageRequest::AutoFetchNotificationState::kUnknown) {
      // Check that the navigation is happening on the tab from which the
      // request came.
      std::optional<TabInfo> tab = tab_finder_->FindNavigationTab(web_contents);
      if (tab && tab->android_tab_id == request.metadata.android_tab_id)
        SetNotificationStateToShown(request.request_id);
    }
  }
}

void InternalImpl::SetNotificationStateToShown(int64_t request_id) {
  const auto kShown = SavePageRequest::AutoFetchNotificationState::kShown;
  for (RequestInfo& request : requests_) {
    if (request.request_id == request_id)
      request.notification_state = kShown;
  }
  delegate_->SetNotificationStateToShown(request_id);
}

void InternalImpl::TabClosed(int android_tab_id) {
  // List of requests is reconciled when the request list is initialized, so
  // ignore if initialization isn't complete.
  if (!requests_initialized_)
    return;

  // Find requests for the closing tab, and ensure the in-progress
  // notification is fired.
  for (RequestInfo& request : requests_) {
    if (request.metadata.android_tab_id == android_tab_id &&
        request.notification_state ==
            SavePageRequest::AutoFetchNotificationState::kUnknown) {
      SetNotificationStateToShown(request.request_id);
    }
  }
}

void InternalImpl::TabModelReady() {
  // Note that typically the tab model is ready immediately, but it's not
  // available when Chrome runs in the background.
  tab_model_ready_ = true;
  if (requests_initialized_)
    UpdateNotificationStateForAllRequests();
}

}  // namespace auto_fetch_internal

// Watches out for tab events, and calls |InternalImpl::TabModelReady| and
// |InternalImpl::TabClosed|.
class AutoFetchPageLoadWatcher::TabWatcher : public TabModelListObserver,
                                             public TabModelObserver {
 public:
  explicit TabWatcher(InternalImpl* impl) : impl_(impl) {
    // PostTask is used to avoid interfering with the tab model while a tab is
    // being created, as this has previously resulted in crashes.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TabWatcher::RegisterTabObserver, GetWeakPtr()));
  }

  ~TabWatcher() override {
    if (observed_tab_model_)
      observed_tab_model_->RemoveObserver(this);
    TabModelList::RemoveObserver(this);
  }

  void RegisterTabObserver() {
    if (!TabModelList::models().empty()) {
      OnTabModelAdded();
    } else {
      TabModelList::AddObserver(this);
    }
  }

  // TabModelObserver.
  void TabPendingClosure(TabAndroid* tab) override {
    impl_->TabClosed(tab->GetAndroidId());
  }

  // TabModelListObserver.
  void OnTabModelAdded() override {
    if (observed_tab_model_)
      return;
    // The assumption is that there can be at most one non-off-the-record tab
    // model. Observe it if it exists.
    for (TabModel* model : TabModelList::models()) {
      if (!model->IsOffTheRecord()) {
        observed_tab_model_ = model;
        observed_tab_model_->AddObserver(this);
        impl_->TabModelReady();
        break;
      }
    }
  }

  void OnTabModelRemoved() override {
    if (!observed_tab_model_)
      return;

    for (const TabModel* remaining_model : TabModelList::models()) {
      if (observed_tab_model_ == remaining_model)
        return;
    }
    observed_tab_model_ = nullptr;
  }

 private:
  base::WeakPtr<TabWatcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  raw_ptr<InternalImpl> impl_;
  // The observed tab model. May be null if not yet observing.
  raw_ptr<TabModel> observed_tab_model_ = nullptr;
  base::WeakPtrFactory<TabWatcher> weak_ptr_factory_{this};
};

AutoFetchPageLoadWatcher::AutoFetchPageLoadWatcher(
    AutoFetchNotifier* notifier,
    RequestCoordinator* request_coordinator,
    std::unique_ptr<AndroidTabFinder> tab_finder)
    : request_coordinator_(request_coordinator),
      impl_(notifier, this, std::move(tab_finder)),
      tab_watcher_(std::make_unique<TabWatcher>(&impl_)) {
  request_coordinator_->AddObserver(this);
  request_coordinator_->GetAllRequests(base::BindOnce(
      &AutoFetchPageLoadWatcher::InitializeRequestList, GetWeakPtr()));
}

AutoFetchPageLoadWatcher::~AutoFetchPageLoadWatcher() {
  request_coordinator_->RemoveObserver(this);
}

void AutoFetchPageLoadWatcher::RemoveRequests(
    const std::vector<int64_t>& request_ids) {
  request_coordinator_->RemoveRequests(request_ids, base::DoNothing());
}

void AutoFetchPageLoadWatcher::HandleNavigation(
    content::NavigationHandle* navigation_handle) {
  // First, call HandleSuccessfulPageNavigation() if this is a successful
  // navigation.
  if (!navigation_handle->IsErrorPage()) {
    // Note: The redirect chain includes the final URL. We consider all URLs
    // along the redirect chain as successful.
    for (const auto& url : navigation_handle->GetRedirectChain()) {
      impl_.SuccessfulPageNavigation(url);
    }
  }

  // Ignore if the URL didn't change.
  const GURL& previous_url =
      navigation_handle->GetPreviousPrimaryMainFrameURL();
  if (navigation_handle->GetURL() == previous_url)
    return;

  impl_.NavigationFrom(previous_url, navigation_handle->GetWebContents());
}

void AutoFetchPageLoadWatcher::SetNotificationStateToShown(int64_t request_id) {
  request_coordinator_->SetAutoFetchNotificationState(
      request_id, SavePageRequest::AutoFetchNotificationState::kShown,
      base::BindOnce(&InternalImpl::SetNotificationStateComplete,
                     impl_.GetWeakPtr(), request_id));
}

void AutoFetchPageLoadWatcher::OnAdded(const SavePageRequest& request) {
  std::optional<RequestInfo> info = MakeRequestInfo(request);
  if (!info)
    return;

  impl_.RequestAdded(std::move(info.value()));
}

void AutoFetchPageLoadWatcher::OnCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  std::optional<RequestInfo> info = MakeRequestInfo(request);
  if (!info)
    return;

  impl_.RequestRemoved(std::move(info.value()));
}

void AutoFetchPageLoadWatcher::InitializeRequestList(
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  std::vector<RequestInfo> request_infos;
  for (const auto& request : requests) {
    std::optional<RequestInfo> info = MakeRequestInfo(*request);
    if (!info)
      continue;
    request_infos.push_back(info.value());
  }
  impl_.RequestListInitialized(std::move(request_infos));
}

}  // namespace offline_pages
