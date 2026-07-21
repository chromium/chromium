// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/twa_launch_queue_tab_helper.h"

#include "chrome/browser/android/webapps/twa_launch_navigation_handle_user_data.h"
#include "chrome/browser/android/webapps/twa_launch_queue_delegate.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/webapps/browser/launch_queue/launch_queue.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace webapps {

TwaLaunchQueueTabHelper::~TwaLaunchQueueTabHelper() = default;

LaunchQueue& TwaLaunchQueueTabHelper::EnsureLaunchQueue() {
  if (!launch_queue_) {
    std::unique_ptr<LaunchQueueDelegate> delegate =
        std::make_unique<TwaLaunchQueueDelegate>();
    launch_queue_ =
        std::make_unique<LaunchQueue>(&GetWebContents(), std::move(delegate));
  }
  return *launch_queue_;
}

void TwaLaunchQueueTabHelper::PrepareForLaunch(
    int64_t launch_token,
    LaunchParams launch_params,
    bool has_speculative_navigation) {
  // Clear any previous pending launches to prevent memory accumulation from
  // aborted or superseded launches.
  pending_launches_.clear();

  CHECK(!pending_launches_.contains(launch_token));
  CHECK(!active_launches_.contains(launch_token));
  CHECK(!committed_launches_.contains(launch_token));
  pending_launches_[launch_token] = {
      std::move(launch_params), DigitalAssetLinksVerificationStatus::kPending,
      has_speculative_navigation};
}

void TwaLaunchQueueTabHelper::OnLaunchVerified(int64_t launch_token,
                                               bool success) {
  DigitalAssetLinksVerificationStatus new_status =
      success ? DigitalAssetLinksVerificationStatus::kSuccess
              : DigitalAssetLinksVerificationStatus::kFailed;

  // Case A: Still pending start (not matched to navigation yet).
  auto pending_it = pending_launches_.find(launch_token);
  if (pending_it != pending_launches_.end()) {
    CHECK(!active_launches_.contains(launch_token));
    CHECK(!committed_launches_.contains(launch_token));
    if (new_status == DigitalAssetLinksVerificationStatus::kSuccess) {
      pending_it->second.status = new_status;
      // Handle speculative loads (e.g. hidden tabs) if we don't expect a new
      // navigation to start. If the navigation already committed before C++ was
      // prepared for the launch (missing the start matching), but the active
      // frame is currently at the target URL and in scope, we can deliver the
      // parameters immediately.
      if (pending_it->second.has_speculative_navigation) {
        content::RenderFrameHost* rfh = GetWebContents().GetPrimaryMainFrame();
        if (rfh && rfh->IsActive() &&
            rfh->GetLastCommittedURL().GetWithoutRef() ==
                pending_it->second.params.target_url().GetWithoutRef() &&
            EnsureLaunchQueue().IsInScope(pending_it->second.params,
                                          rfh->GetLastCommittedURL())) {
          EnsureLaunchQueue().Enqueue(std::move(pending_it->second.params));
          pending_launches_.erase(pending_it);
        }
      }
    } else if (new_status == DigitalAssetLinksVerificationStatus::kFailed) {
      pending_launches_.erase(pending_it);
    }
    return;
  }

  // Case B: Active navigation.
  auto active_it = active_launches_.find(launch_token);
  if (active_it != active_launches_.end()) {
    CHECK(!pending_launches_.contains(launch_token));
    CHECK(!committed_launches_.contains(launch_token));
    auto* user_data = TwaLaunchNavigationHandleUserData::GetForNavigationHandle(
        *active_it->second);
    CHECK(user_data);
    user_data->set_status(new_status);
    if (new_status == DigitalAssetLinksVerificationStatus::kFailed) {
      active_launches_.erase(active_it);
    }
    return;
  }

  // Case C: Already committed, waiting for verification.
  auto committed_it = committed_launches_.find(launch_token);
  if (committed_it != committed_launches_.end()) {
    CHECK(!pending_launches_.contains(launch_token));
    CHECK(!active_launches_.contains(launch_token));
    auto launch = std::move(committed_it->second);
    committed_launches_.erase(committed_it);

    if (new_status != DigitalAssetLinksVerificationStatus::kSuccess) {
      return;
    }
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(launch.rfh_id);
    if (!rfh || !rfh->IsActive()) {
      return;
    }
    if (EnsureLaunchQueue().IsInScope(launch.params,
                                      rfh->GetLastCommittedURL())) {
      EnsureLaunchQueue().Enqueue(std::move(launch.params));
    }
  }
}

void TwaLaunchQueueTabHelper::EnqueueNonNavigating(LaunchParams launch_params) {
  launch_params.set_started_new_navigation(false);
  if (EnsureLaunchQueue().IsInScope(launch_params,
                                    GetWebContents().GetLastCommittedURL())) {
    EnsureLaunchQueue().Enqueue(std::move(launch_params));
  }
}

void TwaLaunchQueueTabHelper::DidStartNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Match by token from ChromeNavigationUIData.
  auto* ui_data =
      static_cast<ChromeNavigationUIData*>(handle->GetNavigationUIData());
  if (!ui_data || !ui_data->twa_launch_token().has_value()) {
    return;
  }

  int64_t token = ui_data->twa_launch_token().value();
  auto it = pending_launches_.find(token);
  if (it != pending_launches_.end()) {
    TwaLaunchNavigationHandleUserData::CreateForNavigationHandle(
        *handle, token, std::move(it->second.params), it->second.status);
    active_launches_[token] = handle;
    pending_launches_.erase(it);
  }
}

void TwaLaunchQueueTabHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame()) {
    return;
  }

  // If a new navigation commits to a new document, any previous pending
  // launches from the prior document state are no longer relevant. Clear them
  // immediately to prevent stale launches accumulating if verification hangs.
  if (handle->HasCommitted() && !handle->IsSameDocument()) {
    committed_launches_.clear();
  }

  // Clean up active_launches_ by matching the raw NavigationHandle pointer.
  // We do this defensively before checking user_data to ensure we don't leave
  // dangling pointers. If the launch is still pending verification, we will
  // transfer its state to committed_launches_ later in this function, so it
  // is safe to remove from active_launches_ here.
  absl::erase_if(active_launches_,
                 [handle](const auto& pair) { return pair.second == handle; });

  auto* user_data =
      TwaLaunchNavigationHandleUserData::GetForNavigationHandle(*handle);

  int64_t token = 0;
  LaunchParams params;
  DigitalAssetLinksVerificationStatus status =
      DigitalAssetLinksVerificationStatus::kPending;
  bool has_token = false;

  if (user_data) {
    token = user_data->launch_token();
    params = std::move(user_data->launch_params());
    status = user_data->status();
    has_token = true;
  } else {
    // Try late matching by token from ChromeNavigationUIData.
    auto* ui_data =
        static_cast<ChromeNavigationUIData*>(handle->GetNavigationUIData());
    if (ui_data && ui_data->twa_launch_token().has_value()) {
      token = ui_data->twa_launch_token().value();
      auto it = pending_launches_.find(token);
      if (it != pending_launches_.end()) {
        params = std::move(it->second.params);
        status = it->second.status;
        pending_launches_.erase(it);
        has_token = true;
      }
    }
  }

  if (has_token) {
    if (!handle->HasCommitted() || handle->IsErrorPage()) {
      return;
    }

    if (!EnsureLaunchQueue().IsInScope(params, handle->GetURL())) {
      return;
    }

    if (status == DigitalAssetLinksVerificationStatus::kSuccess) {
      EnsureLaunchQueue().Enqueue(std::move(params));
    } else if (status == DigitalAssetLinksVerificationStatus::kPending) {
      content::RenderFrameHost* rfh = handle->GetRenderFrameHost();
      if (rfh) {
        CHECK(!committed_launches_.contains(token));
        committed_launches_[token] = {std::move(params), rfh->GetGlobalId()};
      }
    }
    return;
  }

  // Fallback URL matching for speculative loads (no token attached to
  // navigation).
  if (!handle->HasCommitted() || handle->IsErrorPage()) {
    return;
  }
  auto it = std::ranges::find_if(pending_launches_, [&](const auto& pair) {
    return pair.second.has_speculative_navigation &&
           pair.second.params.target_url().GetWithoutRef() ==
               handle->GetURL().GetWithoutRef();
  });
  if (it != pending_launches_.end()) {
    int64_t fallback_token = it->first;
    if (EnsureLaunchQueue().IsInScope(it->second.params, handle->GetURL())) {
      if (it->second.status == DigitalAssetLinksVerificationStatus::kSuccess) {
        EnsureLaunchQueue().Enqueue(std::move(it->second.params));
      } else if (it->second.status ==
                 DigitalAssetLinksVerificationStatus::kPending) {
        content::RenderFrameHost* rfh = handle->GetRenderFrameHost();
        if (rfh) {
          CHECK(!committed_launches_.contains(fallback_token));
          committed_launches_[fallback_token] = {std::move(it->second.params),
                                                 rfh->GetGlobalId()};
        }
      }
    }
    pending_launches_.erase(it);
  }
}

void TwaLaunchQueueTabHelper::FlushLaunchQueueForTesting() const {
  if (!launch_queue_) {
    return;
  }
  launch_queue_->FlushForTesting();  // IN-TEST
}

TwaLaunchQueueTabHelper::TwaLaunchQueueTabHelper(content::WebContents* contents)
    : content::WebContentsUserData<TwaLaunchQueueTabHelper>(*contents),
      content::WebContentsObserver(contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TwaLaunchQueueTabHelper);

}  // namespace webapps
