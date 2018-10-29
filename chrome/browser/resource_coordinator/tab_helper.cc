// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_helper.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_memory_metrics_reporter.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/resource_coordinator/public/cpp/page_resource_coordinator.h"
#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#endif

namespace resource_coordinator {

ResourceCoordinatorTabHelper::ResourceCoordinatorTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  TabLoadTracker::Get()->StartTracking(web_contents);

  service_manager::Connector* connector = nullptr;
  if (content::ServiceManagerConnection::GetForProcess()) {
    connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    page_resource_coordinator_ =
        std::make_unique<resource_coordinator::PageResourceCoordinator>(
            connector);

    // Make sure to set the visibility property when we create
    // |page_resource_coordinator_|.
    const bool is_visible =
        web_contents->GetVisibility() != content::Visibility::HIDDEN;
    page_resource_coordinator_->SetVisibility(is_visible);

    if (auto* page_signal_receiver =
            resource_coordinator::PageSignalReceiver::GetInstance()) {
      // Gets CoordinationUnitID for this WebContents and adds it to
      // PageSignalReceiver.
      page_signal_receiver->AssociateCoordinationUnitIDWithWebContents(
          page_resource_coordinator_->id(), web_contents);
    }

    TabMemoryMetricsReporter::Get()->StartReporting(TabLoadTracker::Get());
  }

#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kSiteCharacteristicsDatabase)) {
    local_site_characteristics_wc_observer_ =
        std::make_unique<LocalSiteCharacteristicsWebContentsObserver>(
            web_contents);
  }
#endif
}

ResourceCoordinatorTabHelper::~ResourceCoordinatorTabHelper() = default;

void ResourceCoordinatorTabHelper::DidStartLoading() {
  if (page_resource_coordinator_)
    page_resource_coordinator_->SetIsLoading(true);
  TabLoadTracker::Get()->DidStartLoading(web_contents());
}

void ResourceCoordinatorTabHelper::DidReceiveResponse() {
  TabLoadTracker::Get()->DidReceiveResponse(web_contents());
}

void ResourceCoordinatorTabHelper::DidStopLoading() {
  if (page_resource_coordinator_)
    page_resource_coordinator_->SetIsLoading(false);
  TabLoadTracker::Get()->DidStopLoading(web_contents());
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
  TabLoadTracker::Get()->RenderProcessGone(web_contents(), status);
}

void ResourceCoordinatorTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (page_resource_coordinator_) {
    // TODO(fdoray): An OCCLUDED tab should not be considered visible.
    const bool is_visible = visibility != content::Visibility::HIDDEN;
    page_resource_coordinator_->SetVisibility(is_visible);
  }
}

void ResourceCoordinatorTabHelper::WebContentsDestroyed() {
  if (page_resource_coordinator_) {
    if (auto* page_signal_receiver =
            resource_coordinator::PageSignalReceiver::GetInstance()) {
      // Gets CoordinationUnitID for this WebContents and removes it from
      // PageSignalReceiver.
      page_signal_receiver->RemoveCoordinationUnitID(
          page_resource_coordinator_->id());
    }
  }
  TabLoadTracker::Get()->StopTracking(web_contents());
}

void ResourceCoordinatorTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (page_resource_coordinator_) {
    // Grab the current time up front, as this is as close as we'll get to the
    // original commit time.
    base::TimeTicks navigation_committed_time = base::TimeTicks::Now();

    content::RenderFrameHost* render_frame_host =
        navigation_handle->GetRenderFrameHost();
    // Make sure the hierarchical structure is constructed before sending signal
    // to Resource Coordinator.
    auto* frame_resource_coordinator =
        render_frame_host->GetFrameResourceCoordinator();
    page_resource_coordinator_->AddFrame(*frame_resource_coordinator);

    auto* process_resource_coordinator =
        render_frame_host->GetProcess()->GetProcessResourceCoordinator();
    process_resource_coordinator->AddFrame(*frame_resource_coordinator);

    if (navigation_handle->IsInMainFrame()) {
      if (auto* page_signal_receiver =
              resource_coordinator::PageSignalReceiver::GetInstance()) {
        // Update the last observed navigation ID for this WebContents.
        page_signal_receiver->SetNavigationID(
            web_contents(), navigation_handle->GetNavigationId());
      }

      UpdateUkmRecorder(navigation_handle->GetNavigationId());
      ResetFlag();
      page_resource_coordinator_->OnMainFrameNavigationCommitted(
          navigation_committed_time, navigation_handle->GetNavigationId(),
          navigation_handle->GetURL().spec());
    }
  }
}

void ResourceCoordinatorTabHelper::TitleWasSet(
    content::NavigationEntry* entry) {
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }
  if (page_resource_coordinator_)
    page_resource_coordinator_->OnTitleUpdated();
}

void ResourceCoordinatorTabHelper::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }
  if (page_resource_coordinator_)
    page_resource_coordinator_->OnFaviconUpdated();
}

void ResourceCoordinatorTabHelper::UpdateUkmRecorder(int64_t navigation_id) {
  ukm_source_id_ =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  if (page_resource_coordinator_)
    page_resource_coordinator_->SetUKMSourceId(ukm_source_id_);
}

void ResourceCoordinatorTabHelper::ResetFlag() {
  first_time_title_set_ = false;
  first_time_favicon_set_ = false;
}

}  // namespace resource_coordinator
