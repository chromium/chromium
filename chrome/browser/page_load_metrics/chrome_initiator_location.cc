// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/chrome_initiator_location.h"

#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "content/public/browser/navigation_handle.h"

void AttachNewTabPageNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      GetInitiatorLocation(ChromeInitiatorLocation::kNewTabPage));
}

void AttachOmniboxDirectUrlInputNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      GetInitiatorLocation(ChromeInitiatorLocation::kOmniboxDirectUrlInput));
}

void AttachOmniboxDefaultSearchEngineNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      GetInitiatorLocation(
          ChromeInitiatorLocation::kOmniboxDefaultSearchEngine));
}

void AttachBookmarkBarNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      GetInitiatorLocation(ChromeInitiatorLocation::kBookmarkBar));
}
