// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_helper.h"

#include <string>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_memory_metrics_reporter.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/resource_coordinator/tab_manager.h"
#endif

namespace resource_coordinator {

ResourceCoordinatorTabHelper::ResourceCoordinatorTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ResourceCoordinatorTabHelper>(
          *web_contents) {
  TabLoadTracker::Get()->StartTracking(web_contents);
  if (memory_instrumentation::MemoryInstrumentation::GetInstance()) {
    auto* rc_parts = g_browser_process->resource_coordinator_parts();
    DCHECK(rc_parts);
    rc_parts->tab_memory_metrics_reporter()->StartReporting(
        TabLoadTracker::Get());
  }
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

void ResourceCoordinatorTabHelper::PrimaryPageChanged(content::Page& page) {
  ukm_source_id_ =
      ukm::ConvertToSourceId(page.GetMainDocument().GetPageUkmSourceId(),
                             ukm::SourceIdType::NAVIGATION_ID);
  TabLoadTracker::Get()->PrimaryPageChanged(web_contents());
}

void ResourceCoordinatorTabHelper::DidStopLoading() {
  TabLoadTracker::Get()->DidStopLoading(web_contents());
}

void ResourceCoordinatorTabHelper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // TODO(siggi): Looks like this can be acquired in a more timely manner from
  //    the RenderProcessHostObserver.
  TabLoadTracker::Get()->RenderProcessGone(web_contents(), status);
}

void ResourceCoordinatorTabHelper::WebContentsDestroyed() {
  TabLoadTracker::Get()->StopTracking(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ResourceCoordinatorTabHelper);

}  // namespace resource_coordinator
