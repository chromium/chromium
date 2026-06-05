// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/twa_launch_queue_tab_helper.h"

#include "chrome/browser/android/webapps/twa_launch_navigation_handle_user_data.h"
#include "chrome/browser/android/webapps/twa_launch_queue_delegate.h"
#include "components/webapps/browser/launch_queue/launch_queue.h"
#include "content/public/browser/navigation_handle.h"

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

void TwaLaunchQueueTabHelper::SetPendingLaunchParams(
    LaunchParams launch_params) {
  pending_launch_params_ = std::move(launch_params);
}

void TwaLaunchQueueTabHelper::DidStartNavigation(
    content::NavigationHandle* handle) {
  if (handle->IsInPrimaryMainFrame() && pending_launch_params_) {
    TwaLaunchNavigationHandleUserData::CreateForNavigationHandle(
        *handle, std::move(*pending_launch_params_));
    pending_launch_params_.reset();
  }
}

void TwaLaunchQueueTabHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (handle->HasCommitted() && !handle->IsErrorPage()) {
    auto* user_data =
        TwaLaunchNavigationHandleUserData::GetForNavigationHandle(*handle);
    if (user_data) {
      EnsureLaunchQueue().Enqueue(std::move(user_data->launch_params()));
    }
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
