// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "components/prerender/browser/prerender_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace resource_coordinator {

// static
std::unique_ptr<BackgroundTabNavigationThrottle>
BackgroundTabNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(features::kStaggeredBackgroundTabOpening))
    return nullptr;

  // Only consider main frames because this is to delay tabs.
  if (!navigation_handle->IsInMainFrame())
    return nullptr;

  content::WebContents* web_contents = navigation_handle->GetWebContents();

  // Never delay foreground tabs.

  // TODO(fdoray): Create a throttle for OCCLUDED WebContents. To do this, it is
  // necessary to support removing the throttle when the WebContents is no
  // longer OCCLUDED (currently, the throttle removed by
  // TabManager::ResumeTabNavigationIfNeeded when the active tab changes).
  // https://crbug.com/810506
  if (web_contents->GetVisibility() != content::Visibility::HIDDEN)
    return nullptr;

  // Never delay the tab when there is opener, so the created window can talk
  // to the creator immediately.
  if (web_contents->HasOpener())
    return nullptr;

  // Only delay the first navigation in a newly created tab.
  if (!web_contents->GetController().IsInitialNavigation())
    return nullptr;

  // Do not delay prerenders.
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (prerender_manager &&
      prerender_manager->IsWebContentsPrerendering(web_contents)) {
    return nullptr;
  }

  // TabUIHelper is initialized in TabHelpers::AttachTabHelpers for every tab
  // and is used to show customized favicon and title for the delayed tab. If
  // the corresponding TabUIHelper is null, it indicates that this WebContents
  // is not a tab, e.g., it can be a BrowserPlugin.
  if (!TabUIHelper::FromWebContents(web_contents))
    return nullptr;

  return std::make_unique<BackgroundTabNavigationThrottle>(navigation_handle);
}

const char* BackgroundTabNavigationThrottle::GetNameForLogging() {
  return "BackgroundTabNavigationThrottle";
}

BackgroundTabNavigationThrottle::BackgroundTabNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

BackgroundTabNavigationThrottle::~BackgroundTabNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
BackgroundTabNavigationThrottle::WillStartRequest() {
  return g_browser_process->GetTabManager()->MaybeThrottleNavigation(this);
}

void BackgroundTabNavigationThrottle::ResumeNavigation() {
  Resume();
}

}  // namespace resource_coordinator
