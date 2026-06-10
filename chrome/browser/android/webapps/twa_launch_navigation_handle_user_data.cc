// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/twa_launch_navigation_handle_user_data.h"

#include "chrome/browser/android/webapps/twa_launch_queue_tab_helper.h"
#include "components/webapps/browser/launch_queue/launch_queue.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace webapps {

TwaLaunchNavigationHandleUserData::~TwaLaunchNavigationHandleUserData() =
    default;

// static
void TwaLaunchNavigationHandleUserData::EnqueueNonNavigating(
    content::WebContents* web_contents,
    LaunchParams launch_params) {
  CHECK(web_contents);
  TwaLaunchQueueTabHelper* tab_helper =
      TwaLaunchQueueTabHelper::GetOrCreateForWebContents(web_contents);
  CHECK(tab_helper);
  launch_params.set_started_new_navigation(false);
  tab_helper->EnsureLaunchQueue().Enqueue(std::move(launch_params));
}

TwaLaunchNavigationHandleUserData::TwaLaunchNavigationHandleUserData(
    content::NavigationHandle& navigation_handle,
    LaunchParams launch_params)
    : launch_params_(std::move(launch_params)) {}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(TwaLaunchNavigationHandleUserData);

}  // namespace webapps
