// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"

#include "components/saved_tab_groups/public/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "net/http/http_request_headers.h"

namespace tab_groups {

// static
bool TabGroupSyncUtils::IsSaveableNavigation(
    content::NavigationHandle* navigation_handle) {
  ui::PageTransition page_transition = navigation_handle->GetPageTransition();

  // The initial request needs to be a GET request, regardless of server-side
  // redirects later on.
  if (navigation_handle->GetRequestMethod() !=
      net::HttpRequestHeaders::kGetMethod) {
    return false;
  }
  if (!ui::IsValidPageTransitionType(page_transition)) {
    return false;
  }
  if (ui::PageTransitionIsRedirect(page_transition)) {
    return false;
  }

  if (!ui::PageTransitionIsMainFrame(page_transition)) {
    return false;
  }

  if (!navigation_handle->HasCommitted()) {
    return false;
  }

  if (!navigation_handle->ShouldUpdateHistory()) {
    return false;
  }

  // For renderer initiated navigation, in most cases these navigations will be
  // auto triggered on restoration. So there is no need to save them.
  if (navigation_handle->IsRendererInitiated() &&
      !navigation_handle->HasUserGesture()) {
    return false;
  }

  return IsURLValidForSavedTabGroups(navigation_handle->GetURL());
}

// statics
void TabGroupSyncUtils::RecordSavedTabGroupNavigationUkmMetrics(
    const LocalTabID& id,
    SavedTabGroupType type,
    content::NavigationHandle* navigation_handle,
    TabGroupSyncService* tab_group_sync_service) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  tab_group_sync_service->GetTabGroupSyncMetricsLogger()
      ->RecordSavedTabGroupNavigation(
          id, navigation_handle->GetURL(), type, navigation_handle->IsPost(),
          navigation_handle->GetRedirectChain().size() > 1,
          navigation_handle->GetRenderFrameHost()->GetPageUkmSourceId());
}

}  // namespace tab_groups
