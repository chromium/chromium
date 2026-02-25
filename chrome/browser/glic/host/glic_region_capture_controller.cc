// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_region_capture_controller.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/widget/widget.h"
#endif

namespace glic {

#if !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/452944608): Add additional metrics for the CaptureRegion API.

GlicRegionCaptureController::GlicRegionCaptureController() = default;

GlicRegionCaptureController::~GlicRegionCaptureController() = default;

void GlicRegionCaptureController::CaptureRegion(
    tabs::TabInterface* tab,
    mojo::PendingRemote<mojom::CaptureRegionObserver> observer) {
  on_capture_region_for_testing_.Run();
  // If a capture is already in progress, cancel it and notify the observer.
  if (capture_region_observer_) {
    capture_region_observer_->OnUpdate(
        mojom::CaptureRegionResultPtr(),
        mojom::CaptureRegionErrorReason::kUnknown);
    capture_region_observer_.reset();
  }
  if (lens_region_search_controller_) {
    // Invalidate weak ptrs to prevent the old controller's callbacks from
    // running.
    weak_factory_.InvalidateWeakPtrs();
    lens_region_search_controller_->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
  content::WebContents* web_contents = tab ? tab->GetContents() : nullptr;
  if (!web_contents) {
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kNoFocusableTab);
    return;
  }
  tab_handle_ = tab->GetHandle();
  content_discarded_subscription_ = tab->RegisterWillDiscardContents(
      base::BindRepeating(&GlicRegionCaptureController::HandleDiscardContents,
                          weak_factory_.GetWeakPtr()));

  capture_region_observer_.Bind(std::move(observer));
  capture_region_observer_.set_disconnect_handler(base::BindOnce(
      &GlicRegionCaptureController::OnCaptureRegionObserverDisconnected,
      base::Unretained(this)));

  if (base::FeatureList::IsEnabled(features::kGlicRegionSelectionNew)) {
    SelectionOverlayController::FromTabWebContents(web_contents)->Show();
  } else {
    lens_region_search_controller_ =
        std::make_unique<lens::LensRegionSearchController>();
    lens_region_search_controller_->StartForRegionSelection(
        web_contents, /*is_multi_capture=*/true,
        base::BindRepeating(&GlicRegionCaptureController::OnRegionSelected,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(
            &GlicRegionCaptureController::OnRegionSelectionFlowClosed,
            weak_factory_.GetWeakPtr()));
  }
}

void GlicRegionCaptureController::ResetMembers() {
  if (base::FeatureList::IsEnabled(features::kGlicRegionSelectionNew) &&
      tab_handle_.Get()) {
    if (auto* web_contents = tab_handle_.Get()->GetContents()) {
      SelectionOverlayController::FromTabWebContents(web_contents)->Hide();
    }
  }

  lens_region_search_controller_.reset();
  capture_region_observer_.reset();
}

void GlicRegionCaptureController::CancelCaptureRegion() {
  if (lens_region_search_controller_) {
    // Calling CloseWithReason() will end the ScreenshotFlow, which will trigger
    // LensRegionSearchController::OnRegionSelectionCompleted() with a failure.
    // This in turn calls HandleCaptureFailure(), which runs the
    // `region_selection_flow_closed_callback_` which is
    // GlicRegionCaptureController::OnRegionSelectionFlowClosed().
    lens_region_search_controller_->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  } else {
    // If there is no controller, there will be no OnRegionSelectionFlowClosed
    // callback, so we need to clean up here.
    ResetMembers();
  }
}

void GlicRegionCaptureController::OnRegionSelected(const gfx::Rect& rect) {
  if (!capture_region_observer_) {
    return;
  }
  content::WebContents* web_contents = nullptr;
  if (auto* tab = tab_handle_.Get()) {
    web_contents = tab->GetContents();
  }
  if (!web_contents) {
    capture_region_observer_->OnUpdate(
        mojom::CaptureRegionResultPtr(),
        mojom::CaptureRegionErrorReason::kUnknown);
    CancelCaptureRegion();
    return;
  }
  auto result = mojom::CaptureRegionResult::New();
  result->tab_id = tab_handle_.raw_value();
  content::RenderWidgetHostView* view =
      web_contents->GetPrimaryMainFrame()->GetView();
  result->region = mojom::CapturedRegion::NewRect(
      gfx::ScaleToEnclosingRect(rect, view->GetDeviceScaleFactor()));
  capture_region_observer_->OnUpdate(std::move(result), std::nullopt);
}

void GlicRegionCaptureController::OnRegionSelectionFlowClosed() {
  // This is called by LensRegionSearchController when the region selection flow
  // is terminated without a successful selection. This can happen if the user
  // cancels the flow (e.g. by pressing Esc or navigating away) or if this
  // controller calls CancelCaptureRegion().
  if (capture_region_observer_) {
    capture_region_observer_->OnUpdate(
        mojom::CaptureRegionResultPtr(),
        mojom::CaptureRegionErrorReason::kUnknown);
  }
  // Post a task to avoid re-entrancy issues where this is called from within
  // the LensRegionSearchController, which we are about to destroy.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&GlicRegionCaptureController::ResetMembers,
                                weak_factory_.GetWeakPtr()));
}

void GlicRegionCaptureController::OnCaptureRegionObserverDisconnected() {
  // The client has closed the pipe, so we should cancel the capture.
  CancelCaptureRegion();
}

void GlicRegionCaptureController::HandleDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  CancelCaptureRegion();
}

bool GlicRegionCaptureController::IsCaptureRegionInProgressForTesting() const {
  return !!lens_region_search_controller_;
}
#else
void GlicRegionCaptureController::CaptureRegion(
    content::WebContents* web_contents,
    mojo::PendingRemote<mojom::CaptureRegionObserver> observer) {
  NOTREACHED();
}
#endif

}  // namespace glic
