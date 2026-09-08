// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/chrome_initiator_location.h"

#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "content/public/browser/navigation_handle.h"

std::string StringifyChromeInitiatorLocation(
    ChromeInitiatorLocation initiator_location) {
  switch (initiator_location) {
    case ChromeInitiatorLocation::kBookmarkBar:
      return "BookmarkBar";
    case ChromeInitiatorLocation::kNewTabPage:
      return "NewTabPage";
    case ChromeInitiatorLocation::kOmniboxDirectUrlInput:
      return "OmniboxDirectUrlInput";
    case ChromeInitiatorLocation::kOmniboxDefaultSearchEngine:
      return "OmniboxDefaultSearchEngine";
    case ChromeInitiatorLocation::kLinkClick:
      return "LinkClick";
    case ChromeInitiatorLocation::kForward:
      return "Forward";
    case ChromeInitiatorLocation::kBackward:
      return "Backward";
    case ChromeInitiatorLocation::kReload:
      return "Reload";
    case ChromeInitiatorLocation::kContextMenuSearch:
      return "ContextMenuSearch";
    case ChromeInitiatorLocation::kOther:
      return "Other";
  }
  NOTREACHED();
}

void AttachNewTabPageNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      GetInitiatorLocation(ChromeInitiatorLocation::kNewTabPage),
      StringifyChromeInitiatorLocation(ChromeInitiatorLocation::kNewTabPage));
}

void AttachOmniboxDirectUrlInputNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      GetInitiatorLocation(ChromeInitiatorLocation::kOmniboxDirectUrlInput),
      StringifyChromeInitiatorLocation(
          ChromeInitiatorLocation::kOmniboxDirectUrlInput));
}

void AttachOmniboxDefaultSearchEngineNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      GetInitiatorLocation(
          ChromeInitiatorLocation::kOmniboxDefaultSearchEngine),
      StringifyChromeInitiatorLocation(
          ChromeInitiatorLocation::kOmniboxDefaultSearchEngine));
}

void AttachBookmarkBarNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      GetInitiatorLocation(ChromeInitiatorLocation::kBookmarkBar),
      StringifyChromeInitiatorLocation(ChromeInitiatorLocation::kBookmarkBar));
}

void AttachContextMenuSearchNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle,
      GetInitiatorLocation(ChromeInitiatorLocation::kContextMenuSearch),
      StringifyChromeInitiatorLocation(
          ChromeInitiatorLocation::kContextMenuSearch));
}

void MarkNavigationServedBySearchPrefetch(
    content::NavigationHandle& navigation_handle) {
  if (auto* user_data =
          page_load_metrics::NavigationHandleUserData::GetForNavigationHandle(
              navigation_handle)) {
    user_data->set_is_served_by_legacy_search_prefetch(true);
  }
}
