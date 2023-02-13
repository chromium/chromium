// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

using content::NavigationEntry;

SupervisedUserNavigationObserver::~SupervisedUserNavigationObserver() {
  supervised_user_service_->RemoveObserver(this);
}

SupervisedUserNavigationObserver::SupervisedUserNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsUserData<SupervisedUserNavigationObserver>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      receivers_(web_contents, this) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  supervised_user_service_ =
      SupervisedUserServiceFactory::GetForProfile(profile);
  url_filter_ = supervised_user_service_->GetURLFilter();
  supervised_user_service_->AddObserver(this);
}

// static
void SupervisedUserNavigationObserver::BindSupervisedUserCommands(
    mojo::PendingAssociatedReceiver<
        supervised_user::mojom::SupervisedUserCommands> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents);
  if (!navigation_observer)
    return;
  navigation_observer->receivers_.Bind(rfh, std::move(receiver));
}

// static
void SupervisedUserNavigationObserver::OnRequestBlocked(
    content::WebContents* web_contents,
    const GURL& url,
    supervised_user::FilteringBehaviorReason reason,
    int64_t navigation_id,
    int frame_id,
    const OnInterstitialResultCallback& callback) {
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents);

  // Cancel the navigation if there is no navigation observer.
  if (!navigation_observer) {
    callback.Run(
        SupervisedUserNavigationThrottle::CallbackActions::kCancelNavigation,
        /* already_requested_permission */ false, /* is_main_frame */ false);
    return;
  }

  navigation_observer->OnRequestBlockedInternal(url, reason, navigation_id,
                                                frame_id, callback);
}

void SupervisedUserNavigationObserver::UpdateMainFrameFilteringStatus(
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user::FilteringBehaviorReason reason) {
  main_frame_filtering_behavior_ = behavior;
  main_frame_filtering_behavior_reason_ = reason;
}

void SupervisedUserNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  int frame_id = navigation_handle->GetFrameTreeNodeId();
  int64_t navigation_id = navigation_handle->GetNavigationId();

  // If this is a different navigation than the one that triggered the
  // interstitial in the frame, then interstitial is done.
  if (base::Contains(supervised_user_interstitials_, frame_id) &&
      navigation_id != supervised_user_interstitials_[frame_id]
                           ->interstitial_navigation_id()) {
    OnInterstitialDone(frame_id);
  }

  // Only filter same page navigations (eg. pushState/popState); others will
  // have been filtered by the NavigationThrottle.
  if (navigation_handle->IsSameDocument() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    auto* render_frame_host = web_contents()->GetPrimaryMainFrame();
    int process_id = render_frame_host->GetProcess()->GetID();
    int routing_id = render_frame_host->GetRoutingID();
    bool skip_manual_parent_filter =
        url_filter_->ShouldSkipParentManualAllowlistFiltering(web_contents());
    url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
        web_contents()->GetLastCommittedURL(),
        base::BindOnce(
            &SupervisedUserNavigationObserver::URLFilterCheckCallback,
            weak_ptr_factory_.GetWeakPtr(), navigation_handle->GetURL(),
            process_id, routing_id),
        skip_manual_parent_filter);
  }
}

void SupervisedUserNavigationObserver::FrameDeleted(int frame_tree_node_id) {
  supervised_user_interstitials_.erase(frame_tree_node_id);
}

void SupervisedUserNavigationObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->IsInPrimaryMainFrame()) {
    bool main_frame_blocked =
        base::Contains(supervised_user_interstitials_,
                       render_frame_host->GetFrameTreeNodeId());
    int count = supervised_user_interstitials_.size();
    if (main_frame_blocked)
      count = 0;

    UMA_HISTOGRAM_COUNTS_1000("ManagedUsers.BlockedIframeCount", count);
  }

  if (base::Contains(supervised_user_interstitials_,
                     render_frame_host->GetFrameTreeNodeId())) {
    UMA_HISTOGRAM_COUNTS_1000("ManagedUsers.BlockedFrameDepth",
                              render_frame_host->GetFrameDepth());
  }
}

void SupervisedUserNavigationObserver::OnURLFilterChanged() {
  auto* main_frame = web_contents()->GetPrimaryMainFrame();
  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int routing_id = main_frame->GetRoutingID();
  bool skip_manual_parent_filter =
      url_filter_->ShouldSkipParentManualAllowlistFiltering(web_contents());
  url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
      web_contents()->GetLastCommittedURL(),
      base::BindOnce(&SupervisedUserNavigationObserver::URLFilterCheckCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents()->GetLastCommittedURL(),
                     main_frame_process_id, routing_id),
      skip_manual_parent_filter);

  MaybeUpdateRequestedHosts();

  // Iframe filtering has been enabled.
  main_frame->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        FilterRenderFrame(render_frame_host);
      });
}

void SupervisedUserNavigationObserver::OnInterstitialDone(int frame_id) {
  supervised_user_interstitials_.erase(frame_id);
}

void SupervisedUserNavigationObserver::OnRequestBlockedInternal(
    const GURL& url,
    supervised_user::FilteringBehaviorReason reason,
    int64_t navigation_id,
    int frame_id,
    const OnInterstitialResultCallback& callback) {
  // TODO(bauerb): Use SaneTime when available.
  base::Time timestamp = base::Time::Now();
  // Create a history entry for the attempt and mark it as such.  This history
  // entry should be marked as "not hidden" so the user can see attempted but
  // blocked navigations.  (This is in contrast to the normal behavior, wherein
  // Chrome marks navigations that result in an error as hidden.)  This is to
  // show the user the same thing that the custodian will see on the dashboard
  // (where it gets via a different mechanism unrelated to history).
  history::HistoryAddPageArgs add_page_args(
      url, timestamp, history::ContextIDForWebContents(web_contents()),
      /*nav_entry_id=*/0, /*referrer=*/url, history::RedirectList(),
      ui::PAGE_TRANSITION_BLOCKED, /*hidden=*/false, history::SOURCE_BROWSED,
      /*did_replace_entry=*/false, /*consider_for_ntp_most_visited=*/true);

  // Add the entry to the history database.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);

  // |history_service| is null if saving history is disabled.
  if (history_service)
    history_service->AddPage(add_page_args);

  std::unique_ptr<NavigationEntry> entry = NavigationEntry::Create();
  entry->SetVirtualURL(url);
  entry->SetTimestamp(timestamp);
  auto serialized_entry = std::make_unique<sessions::SerializedNavigationEntry>(
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          blocked_navigations_.size(), entry.get()));
  blocked_navigations_.push_back(std::move(serialized_entry));

  // Show the interstitial.
  const bool initial_page_load = true;
  MaybeShowInterstitial(url, reason, initial_page_load, navigation_id, frame_id,
                        callback);
}

void SupervisedUserNavigationObserver::URLFilterCheckCallback(
    const GURL& url,
    int render_frame_process_id,
    int render_frame_routing_id,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user::FilteringBehaviorReason reason,
    bool uncertain) {
  auto* render_frame_host = content::RenderFrameHost::FromID(
      render_frame_process_id, render_frame_routing_id);

  // `render_frame_host` could be in an inactive state since this callback is
  // called asynchronously, and we should not reload an unrelated document.
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive() ||
      !render_frame_host->IsActive()) {
    return;
  }

  int frame_id = render_frame_host->GetFrameTreeNodeId();
  bool is_showing_interstitial =
      base::Contains(supervised_user_interstitials_, frame_id);
  bool should_show_interstitial =
      behavior == SupervisedUserURLFilter::FilteringBehavior::BLOCK;

  // If an interstitial is being shown where it shouldn't (for e.g. because a
  // parent just approved a request) reloading will clear it. On the other hand,
  // if an interstitial error page is not being shown but it should be shown,
  // then reloading will trigger the navigation throttle to show the error page.
  if (is_showing_interstitial != should_show_interstitial) {
    if (render_frame_host->IsInPrimaryMainFrame()) {
      web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                             /* check_for_repost */ false);
      return;
    }
    render_frame_host->Reload();
  }
}

void SupervisedUserNavigationObserver::MaybeShowInterstitial(
    const GURL& url,
    supervised_user::FilteringBehaviorReason reason,
    bool initial_page_load,
    int64_t navigation_id,
    int frame_id,
    const OnInterstitialResultCallback& callback) {
  std::unique_ptr<SupervisedUserInterstitial> interstitial =
      SupervisedUserInterstitial::Create(web_contents(), url, reason, frame_id,
                                         navigation_id);

  supervised_user_interstitials_[frame_id] = std::move(interstitial);

  bool already_requested = base::Contains(requested_hosts_, url.host());
  bool is_main_frame =
      frame_id == web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId();

  callback.Run(SupervisedUserNavigationThrottle::CallbackActions::
                   kCancelWithInterstitial,
               already_requested, is_main_frame);
}

void SupervisedUserNavigationObserver::FilterRenderFrame(
    content::RenderFrameHost* render_frame_host) {
  // If the RenderFrameHost is not live return.
  // If the RenderFrameHost belongs to the main frame, return. This is because
  // the main frame is already filtered in
  // |SupervisedUserNavigationObserver::OnURLFilterChanged|.
  if (!render_frame_host->IsRenderFrameLive() ||
      render_frame_host->IsInPrimaryMainFrame())
    return;

  const GURL& last_committed_url = render_frame_host->GetLastCommittedURL();
  url_filter_->GetFilteringBehaviorForSubFrameURLWithAsyncChecks(
      last_committed_url, web_contents()->GetLastCommittedURL(),
      base::BindOnce(&SupervisedUserNavigationObserver::URLFilterCheckCallback,
                     weak_ptr_factory_.GetWeakPtr(), last_committed_url,
                     render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID()));
}

void SupervisedUserNavigationObserver::GoBack() {
  auto* render_frame_host = receivers_.GetCurrentTargetFrame();
  auto id = render_frame_host->GetFrameTreeNodeId();

  // Request can come only from the main frame.
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;

  if (base::Contains(supervised_user_interstitials_, id))
    supervised_user_interstitials_[id]->GoBack();
}

void SupervisedUserNavigationObserver::RequestUrlAccessRemote(
    RequestUrlAccessRemoteCallback callback) {
  auto* render_frame_host = receivers_.GetCurrentTargetFrame();
  int id = render_frame_host->GetFrameTreeNodeId();

  if (!base::Contains(supervised_user_interstitials_, id)) {
    DLOG(WARNING) << "Interstitial with id not found: " << id;
    return;
  }

  SupervisedUserInterstitial* interstitial =
      supervised_user_interstitials_[id].get();
  interstitial->RequestUrlAccessRemote(
      base::BindOnce(&SupervisedUserNavigationObserver::RequestCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     interstitial->url().host()));
}

void SupervisedUserNavigationObserver::RequestUrlAccessLocal(
    RequestUrlAccessLocalCallback callback) {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();
  int id = render_frame_host->GetFrameTreeNodeId();

  if (!base::Contains(supervised_user_interstitials_, id)) {
    DLOG(WARNING) << "Interstitial with id not found: " << id;
    return;
  }

  SupervisedUserInterstitial* interstitial =
      supervised_user_interstitials_[id].get();
  interstitial->RequestUrlAccessLocal(std::move(callback));
}

void SupervisedUserNavigationObserver::Feedback() {
  auto* render_frame_host = receivers_.GetCurrentTargetFrame();
  int id = render_frame_host->GetFrameTreeNodeId();

  if (base::Contains(supervised_user_interstitials_, id))
    supervised_user_interstitials_[id]->ShowFeedback();
}

void SupervisedUserNavigationObserver::RequestCreated(
    RequestUrlAccessRemoteCallback callback,
    const std::string& host,
    bool successfully_created_request) {
  if (successfully_created_request)
    requested_hosts_.insert(host);
  std::move(callback).Run(successfully_created_request);
}

void SupervisedUserNavigationObserver::MaybeUpdateRequestedHosts() {
  SupervisedUserURLFilter::FilteringBehavior filtering_behavior;

  for (auto iter = requested_hosts_.begin(); iter != requested_hosts_.end();) {
    bool is_manual = url_filter_->GetManualFilteringBehaviorForURL(
        GURL(*iter), &filtering_behavior);

    if (is_manual && filtering_behavior ==
                         SupervisedUserURLFilter::FilteringBehavior::ALLOW) {
      iter = requested_hosts_.erase(iter);
    } else {
      iter++;
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SupervisedUserNavigationObserver);
