// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_zoom_controller.h"

#include <algorithm>
#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_webui.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace glic {

namespace {

class Metrics {
 public:
  void RecordZoomIn(bool at_max, ZoomSource source) {
    GlicZoomAction action =
        at_max ? GlicZoomAction::kZoomInAtMax : GlicZoomAction::kZoomIn;
    base::RecordAction(at_max ? base::UserMetricsAction("Glic.ZoomInAtMax")
                              : base::UserMetricsAction("Glic.ZoomIn"));
    RecordAction(action, source);
  }

  void RecordZoomOut(bool at_min, ZoomSource source) {
    GlicZoomAction action =
        at_min ? GlicZoomAction::kZoomOutAtMin : GlicZoomAction::kZoomOut;
    base::RecordAction(at_min ? base::UserMetricsAction("Glic.ZoomOutAtMin")
                              : base::UserMetricsAction("Glic.ZoomOut"));
    RecordAction(action, source);
  }

  void RecordZoomReset(ZoomSource source) {
    base::RecordAction(base::UserMetricsAction("Glic.ZoomReset"));
    RecordAction(GlicZoomAction::kReset, source);
  }

 private:
  void RecordAction(GlicZoomAction action, ZoomSource source) {
    base::UmaHistogramEnumeration("Glic.ZoomAction", action);

    const char* source_histogram = nullptr;
    switch (source) {
      case ZoomSource::kHotkey:
        source_histogram = "Glic.ZoomAction.Hotkey";
        break;
      case ZoomSource::kHotkeyWithShift:
        source_histogram = "Glic.ZoomAction.HotkeyWithShift";
        break;
      case ZoomSource::kScroll:
        source_histogram = "Glic.ZoomAction.Scroll";
        break;
    }
    if (source_histogram) {
      base::UmaHistogramEnumeration(source_histogram, action);
    }
  }
};

std::optional<double> FindNextZoomInFactor(double current_zoom) {
  int current_percent = static_cast<int>(std::round(current_zoom * 100.0));
  for (double factor : GlicZoomController::kZoomFactors) {
    int factor_percent = static_cast<int>(std::round(factor * 100.0));
    if (factor_percent > current_percent) {
      return factor;
    }
  }
  return std::nullopt;
}

std::optional<double> FindNextZoomOutFactor(double current_zoom) {
  int current_percent = static_cast<int>(std::round(current_zoom * 100.0));
  for (auto it = GlicZoomController::kZoomFactors.rbegin();
       it != GlicZoomController::kZoomFactors.rend(); ++it) {
    int factor_percent = static_cast<int>(std::round(*it * 100.0));
    if (factor_percent < current_percent) {
      return *it;
    }
  }
  return std::nullopt;
}

}  // namespace

GlicZoomController::GlicZoomController(
    content::WebContents* guest_contents,
    PrefService* pref_service,
    base::RepeatingClosure on_zoom_change_callback)
    : content::WebContentsObserver(guest_contents),
      pref_service_(pref_service),
      on_zoom_change_callback_(std::move(on_zoom_change_callback)) {
  ApplyZoom();
}

GlicZoomController::~GlicZoomController() {
  if (web_contents()) {
    if (auto* rfh = web_contents()->GetPrimaryMainFrame()) {
      if (rfh->IsRenderFrameLive()) {
        content::HostZoomMap::Get(rfh->GetSiteInstance())
            ->ClearTemporaryZoomLevel(rfh->GetGlobalId());
      }
    }
  }
}

double GlicZoomController::GetCurrentZoomFactor() const {
  int zoom_percent = 100;
  if (pref_service_) {
    zoom_percent = pref_service_->GetInteger(prefs::kGlicZoomLevel);
  }
  if (zoom_percent <= 0) {
    zoom_percent = 100;
  }
  zoom_percent = std::clamp(zoom_percent, 100, 200);
  return zoom_percent / 100.0;
}

void GlicZoomController::ApplyZoom() {
  if (!web_contents()) {
    return;
  }
  auto* rfh = web_contents()->GetPrimaryMainFrame();
  if (!rfh || !rfh->IsRenderFrameLive()) {
    return;
  }
  double factor = GetCurrentZoomFactor();
  double level = blink::ZoomFactorToZoomLevel(factor);
  content::HostZoomMap::Get(rfh->GetSiteInstance())
      ->SetTemporaryZoomLevel(rfh->GetGlobalId(), level);
}

void GlicZoomController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  ApplyZoom();
}

void GlicZoomController::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // When the renderer recovers/reloads, DidFinishNavigation will re-apply the
  // temporary zoom level to the new RenderFrameHost.
}

void GlicZoomController::Zoom(mojom::ZoomAction zoom_action,
                              ZoomSource source) {
  double current_factor = GetCurrentZoomFactor();
  int current_percent = static_cast<int>(std::round(current_factor * 100.0));

  Metrics metrics;
  std::optional<double> target_factor;

  switch (zoom_action) {
    case mojom::ZoomAction::kZoomIn:
      if (current_percent < 200) {
        metrics.RecordZoomIn(/*at_max=*/false, source);
        target_factor = FindNextZoomInFactor(current_factor);
      } else {
        metrics.RecordZoomIn(/*at_max=*/true, source);
      }
      break;
    case mojom::ZoomAction::kZoomOut:
      if (current_percent > 100) {
        metrics.RecordZoomOut(/*at_min=*/false, source);
        target_factor = FindNextZoomOutFactor(current_factor);
      } else {
        metrics.RecordZoomOut(/*at_min=*/true, source);
      }
      break;
    case mojom::ZoomAction::kReset:
      metrics.RecordZoomReset(source);
      target_factor = 1.0;
      break;
  }

  if (target_factor) {
    double new_factor = *target_factor;
    int new_percent = static_cast<int>(std::round(new_factor * 100.0));
    if (pref_service_) {
      pref_service_->SetInteger(prefs::kGlicZoomLevel, new_percent);
    }
    ApplyZoom();
    if (new_percent != current_percent && on_zoom_change_callback_) {
      on_zoom_change_callback_.Run();
    }
  }
}

}  // namespace glic
