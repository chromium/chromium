// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_zoom_controller.h"

#include "base/metrics/histogram_functions.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace {

PreviewZoomController::ZoomUsage TransiteZoomUsage(
    double default_zoom_level,
    PreviewZoomController::ZoomUsage current_zoom_usage,
    double next_zoom_level) {
  switch (current_zoom_usage) {
    case PreviewZoomController::ZoomUsage::kOnlyDefaultUsed:
      if (next_zoom_level == default_zoom_level) {
        return PreviewZoomController::ZoomUsage::kOnlyDefaultUsed;
      }

      return PreviewZoomController::ZoomUsage::kNonDefaultUsed;
    case PreviewZoomController::ZoomUsage::kNonDefaultUsed:
      return PreviewZoomController::ZoomUsage::kNonDefaultUsed;
  }
}

}  // namespace

PreviewZoomController::PreviewZoomController(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PreviewZoomController::~PreviewZoomController() {
  MaybeReportMetrics();
}

void PreviewZoomController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(b:291842891): We will update zoom settings also at the preview
  // navigation.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  was_page_shown_ = true;
  InitializeZoom();
}

void PreviewZoomController::InitializeZoom() {
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  auto* host_zoom_map = content::HostZoomMap::GetForWebContents(web_contents());
  CHECK(zoom_controller);
  CHECK(host_zoom_map);

  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  CHECK(entry);
  const GURL url = host_zoom_map->GetURLFromEntry(entry);
  const std::string host = net::GetHostOrSpecFromURL(url);

  // If a user changed zoom level for the host in this session, recover it.
  // If not, use the default value.
  const double level = host_zoom_map->GetZoomLevelForPreviewAndHost(host);

  // ZoomController::DidFinishNavigation resets zoom mode via
  // ResetZoomModeOnNavigationIfNeeded when a primary main frame navigation
  // happens. So, we need to reset zoom mode every time.
  zoom_controller->SetZoomMode(
      zoom::ZoomController::ZoomMode::ZOOM_MODE_ISOLATED);
  // Set temporary zoom level via ZoomMode::ZOOM_MODE_ISOLATED.
  zoom_controller->SetZoomLevel(level);
  zoom_usage_ = TransiteZoomUsage(zoom_controller->GetDefaultZoomLevel(),
                                  zoom_usage_, level);
}

void PreviewZoomController::ResetZoomForActivation() {
  // Reset zoom mode as default, as if the navigation has just happened.

  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());

  // If we change ZoomMode ZOOM_MODE_ISOLATED to ZOOM_MODE_DEFAULT,
  // ZoomController::SetZoomMode uses persisted zoom level (not for preview) if
  // exists, or temporary zoom level (and persist it) if not. To prevent wired
  // behavior for the latter case, reset temporary zoom level before calling
  // SetZoomMode.
  const double level = zoom_controller->GetDefaultZoomLevel();
  zoom_controller->SetZoomLevel(level);

  zoom_controller->SetZoomMode(
      zoom::ZoomController::ZoomMode::ZOOM_MODE_DEFAULT);
}

double GetNextZoomLevel(double default_zoom_level,
                        double current_zoom_level,
                        content::PageZoom zoom) {
  // Mimic of PageZoom::Zoom.

  std::vector<double> zoom_levels =
      zoom::PageZoom::PresetZoomLevels(default_zoom_level);

  switch (zoom) {
    case content::PAGE_ZOOM_RESET:
      return default_zoom_level;
    case content::PAGE_ZOOM_OUT: {
      auto begin = zoom_levels.crbegin();
      auto end = zoom_levels.crend();
      auto next =
          std::upper_bound(begin, end, current_zoom_level, std::greater<>());
      // If the next level is within epsilon of the current, keep going until
      // we're taking a meaningful step.
      while (next != end && blink::ZoomValuesEqual(*next, current_zoom_level)) {
        ++next;
      }
      if (next == end) {
        --next;
      }

      return *next;
    }
    case content::PAGE_ZOOM_IN: {
      auto begin = zoom_levels.cbegin();
      auto end = zoom_levels.cend();
      auto next = std::upper_bound(begin, end, current_zoom_level);
      // If the next level is within epsilon of the current, keep going until
      // we're taking a meaningful step.
      while (next != end && blink::ZoomValuesEqual(*next, current_zoom_level)) {
        ++next;
      }
      if (next == end) {
        --next;
      }

      return *next;
    }
  }
}

void PreviewZoomController::Zoom(content::PageZoom zoom) {
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  auto* host_zoom_map = content::HostZoomMap::GetForWebContents(web_contents());
  CHECK(zoom_controller);
  CHECK(host_zoom_map);

  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  CHECK(entry);
  const GURL url = host_zoom_map->GetURLFromEntry(entry);
  const std::string host = net::GetHostOrSpecFromURL(url);

  const double level = GetNextZoomLevel(
      zoom_controller->GetDefaultZoomLevel(),
      host_zoom_map->GetZoomLevelForPreviewAndHost(host), zoom);

  // Set temporary zoom level via ZoomMode::ZOOM_MODE_ISOLATED.
  zoom_controller->SetZoomLevel(level);
  // Memorize it in memory.
  host_zoom_map->SetZoomLevelForPreviewAndHost(host, level);
  zoom_usage_ = TransiteZoomUsage(zoom_controller->GetDefaultZoomLevel(),
                                  zoom_usage_, level);
}

void PreviewZoomController::MaybeReportMetrics() {
  if (!was_page_shown_) {
    return;
  }

  base::UmaHistogramEnumeration("LinkPreview.Experimental.ZoomUsage",
                                zoom_usage_);
}
