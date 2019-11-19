// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_helper.h"

#include <string>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_memory_metrics_reporter.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#endif

namespace resource_coordinator {

ResourceCoordinatorTabHelper::ResourceCoordinatorTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  TabLoadTracker::Get()->StartTracking(web_contents);
  if (memory_instrumentation::MemoryInstrumentation::GetInstance()) {
    auto* rc_parts = g_browser_process->resource_coordinator_parts();
    DCHECK(rc_parts);
    rc_parts->tab_memory_metrics_reporter()->StartReporting(
        TabLoadTracker::Get());
  }

#if !defined(OS_ANDROID)
  // Don't create the LocalSiteCharacteristicsWebContentsObserver for this tab
  // if the feature is disabled.
  if (base::FeatureList::IsEnabled(features::kSiteCharacteristicsDatabase)) {
    local_site_characteristics_wc_observer_ =
        std::make_unique<LocalSiteCharacteristicsWebContentsObserver>(
            web_contents);
  }
#endif
}

ResourceCoordinatorTabHelper::~ResourceCoordinatorTabHelper() = default;

bool ResourceCoordinatorTabHelper::IsLoaded(content::WebContents* contents) {
  if (resource_coordinator::ResourceCoordinatorTabHelper::FromWebContents(
          contents)) {
    return resource_coordinator::TabLoadTracker::Get()->GetLoadingState(
               contents) == ::mojom::LifecycleUnitLoadingState::LOADED;
  }
  return true;
}

bool ResourceCoordinatorTabHelper::IsFrozen(content::WebContents* contents) {
#if !defined(OS_ANDROID)
  if (resource_coordinator::ResourceCoordinatorTabHelper::FromWebContents(
          contents)) {
    auto* tab_lifecycle_unit = resource_coordinator::TabLifecycleUnitSource::
        GetTabLifecycleUnitExternal(contents);
    if (tab_lifecycle_unit)
      return tab_lifecycle_unit->IsFrozen();
  }
#endif
  return false;
}

void ResourceCoordinatorTabHelper::DidStartLoading() {
  TabLoadTracker::Get()->DidStartLoading(web_contents());
}

void ResourceCoordinatorTabHelper::DidReceiveResponse() {
  TabLoadTracker::Get()->DidReceiveResponse(web_contents());
}

void ResourceCoordinatorTabHelper::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  TabLoadTracker::Get()->DidFailLoad(web_contents());
}

void ResourceCoordinatorTabHelper::RenderProcessGone(
    base::TerminationStatus status) {
  // TODO(siggi): Looks like this can be acquired in a more timely manner from
  //    the RenderProcessHostObserver.
  TabLoadTracker::Get()->RenderProcessGone(web_contents(), status);
}

void ResourceCoordinatorTabHelper::WebContentsDestroyed() {
  TabLoadTracker::Get()->StopTracking(web_contents());
}

void ResourceCoordinatorTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (navigation_handle->IsInMainFrame()) {
    ukm_source_id_ = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ResourceCoordinatorTabHelper)

}  // namespace resource_coordinator
