// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/region_search/lens_region_search_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/image_editor/screenshot_flow.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/lens/lens_region_search_instructions_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace {

views::Widget* OpenLensRegionSearchInstructions(
    Browser* browser,
    base::OnceClosure close_callback,
    base::OnceClosure escape_callback,
    int text_message_id) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  CHECK(browser_view);
  // Our anchor should be the browser view's top container view. This makes sure
  // that we account for side panel width and the top container view.
  views::View* anchor = browser_view->contents_web_view();

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  lens::LensRegionSearchInstructionsView::LayoutParams layout_params{
      .left_margin = layout_provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL),
      .label_button_insets = layout_provider->GetInsetsMetric(
          views::InsetsMetric::INSETS_LABEL_BUTTON),
      .close_button_margin = layout_provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_CLOSE_BUTTON_MARGIN),
      .vector_icon_size = layout_provider->GetDistanceMetric(
          views::DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE),
      .label_horizontal_distance = layout_provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL),
      .vertical_distance = layout_provider->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)};

  return views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<lens::LensRegionSearchInstructionsView>(
          anchor, std::move(close_callback), std::move(escape_callback),
          layout_params, text_message_id));
}
}  // namespace
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

namespace lens {

LensRegionSearchController::LensRegionSearchController() {
  weak_this_ = weak_factory_.GetWeakPtr();
}

LensRegionSearchController::~LensRegionSearchController() {
  CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

void LensRegionSearchController::Start(
    content::WebContents* web_contents,
    bool use_fullscreen_capture,
    bool is_google_default_search_provider,
    lens::AmbientSearchEntryPoint entry_point) {
  entry_point_ = entry_point;
  is_google_default_search_provider_ = is_google_default_search_provider;
  is_multi_capture_ = false;
  bounds_callback_.Reset();
  region_selection_flow_closed_callback_ = base::DoNothing();
  StartCaptureInternal(chrome::FindBrowserWithTab(web_contents),
                       use_fullscreen_capture);
}

void LensRegionSearchController::StartForRegionSelection(
    content::WebContents* web_contents,
    bool is_multi_capture,
    BoundsCallback bounds_callback,
    RegionSelectionFlowClosedCallback region_selection_flow_closed_callback) {
  is_multi_capture_ = is_multi_capture;
  bounds_callback_ = std::move(bounds_callback);
  region_selection_flow_closed_callback_ =
      std::move(region_selection_flow_closed_callback);

  // Return early if web contents/browser don't exist and if capture mode is
  // already active.
  if (!web_contents || in_capture_mode_) {
    return;
  }

  is_closing_ = false;
  Observe(web_contents);
  if (!screenshot_flow_) {
    screenshot_flow_ =
        std::make_unique<image_editor::ScreenshotFlow>(web_contents);
  }

  in_capture_mode_ = true;
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  // Create user education bubble anchored to the toolbar container.
  bubble_widget_ = OpenLensRegionSearchInstructions(
      chrome::FindBrowserWithTab(web_contents),
      base::BindOnce(&LensRegionSearchController::Close,
                     base::Unretained(this)),
      base::BindOnce(&LensRegionSearchController::Escape,
                     base::Unretained(this)),
      IDS_CAPTURE_REGION_BUBBLE_TEXT);
  bubble_widget_->Show();
#endif
  screenshot_flow_->StartForRegionSelection(base::BindOnce(
      &LensRegionSearchController::OnRegionSelectionCompleted, weak_this_));
}

void LensRegionSearchController::RecordCaptureResult(
    lens::LensRegionSearchCaptureResult result) {
  UMA_HISTOGRAM_ENUMERATION(lens::kLensRegionSearchCaptureResultHistogramName,
                            result);
}

int LensRegionSearchController::CalculateViewportProportionFromAreas(
    int screen_height,
    int screen_width,
    int image_width,
    int image_height) {
  // To get the region proportion of the screen, we must calculate the areas of
  // the screen and captured region. Then, we must divide the area of the region
  // by the area of the screen to get the percentage. Multiply by 100 to make it
  // an integer. Returns -1 if screen_area is 0 to prevent undefined values.
  double screen_area = screen_width * screen_height;
  if (screen_area <= 0) {
    return -1;
  }
  double region_area = image_width * image_height;
  double region_proportion = region_area / screen_area;
  int region_proportion_int = region_proportion * 100;
  return region_proportion_int;
}

lens::LensRegionSearchAspectRatio
LensRegionSearchController::GetAspectRatioFromSize(int image_height,
                                                   int image_width) {
  // Convert to double to prevent integer division.
  double width = image_width;
  double height = image_height;

  // To record region aspect ratio, we must divide the region's width by height.
  // Should never be zero, but check to prevent any crashes or undefined
  // recordings.
  if (height > 0) {
    double aspect_ratio = width / height;
    if (aspect_ratio <= 1.2 && aspect_ratio >= 0.8) {
      return lens::LensRegionSearchAspectRatio::SQUARE;
    } else if (aspect_ratio < 0.8 && aspect_ratio >= 0.3) {
      return lens::LensRegionSearchAspectRatio::TALL;
    } else if (aspect_ratio < 0.3) {
      return lens::LensRegionSearchAspectRatio::VERY_TALL;
    } else if (aspect_ratio > 1.2 && aspect_ratio <= 1.7) {
      return lens::LensRegionSearchAspectRatio::WIDE;
    } else if (aspect_ratio > 1.7) {
      return lens::LensRegionSearchAspectRatio::VERY_WIDE;
    }
  }
  return lens::LensRegionSearchAspectRatio::UNDEFINED;
}

void LensRegionSearchController::RecordRegionSizeRelatedMetrics(
    gfx::Rect screen_bounds,
    gfx::Size image_size) {
  // If any of the rects are empty, it means the area is zero. In this case,
  // return.
  if (screen_bounds.IsEmpty() || image_size.IsEmpty()) {
    return;
  }
  double region_width = image_size.width();
  double region_height = image_size.height();

  int region_proportion = CalculateViewportProportionFromAreas(
      screen_bounds.height(), screen_bounds.width(), region_width,
      region_height);
  if (region_proportion >= 0) {
    base::UmaHistogramPercentage(
        lens::kLensRegionSearchRegionViewportProportionHistogramName,
        region_proportion);
  }

  // To record region aspect ratio, we must divide the region's width by height.
  base::UmaHistogramEnumeration(
      lens::kLensRegionSearchRegionAspectRatioHistogramName,
      GetAspectRatioFromSize(region_height, region_width));
}

void LensRegionSearchController::StartCaptureInternal(
    Browser* browser,
    bool use_fullscreen_capture) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // Return early if web contents/browser don't exist and if capture mode is
  // already active.
  if (!web_contents || in_capture_mode_) {
    return;
  }

  is_closing_ = false;
  Observe(web_contents);
  if (!screenshot_flow_) {
    screenshot_flow_ =
        std::make_unique<image_editor::ScreenshotFlow>(web_contents);
  }

  auto callback = base::BindOnce(
      &LensRegionSearchController::OnCaptureCompleted, weak_this_);
  in_capture_mode_ = true;
  if (use_fullscreen_capture) {
    screenshot_flow_->StartFullscreenCapture(std::move(callback));
  } else {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
    // Create user education bubble anchored to the toolbar container.
    // This is only done for non-fulllscreen capture.
    bubble_widget_ = OpenLensRegionSearchInstructions(
        browser,
        base::BindOnce(&LensRegionSearchController::Close,
                       base::Unretained(this)),
        base::BindOnce(&LensRegionSearchController::Escape,
                       base::Unretained(this)),
        IDS_LENS_REGION_SEARCH_BUBBLE_TEXT);
    bubble_widget_->Show();
#endif
    screenshot_flow_->Start(std::move(callback));
  }
}

void LensRegionSearchController::OnCaptureCompleted(
    const image_editor::ScreenshotCaptureResult& result) {
  if (!is_multi_capture_) {
    // Close all open UI overlays and bubbles.
    CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }

  image_editor::ScreenshotCaptureResultCode code = result.result_code;
  if (code == image_editor::ScreenshotCaptureResultCode::USER_NAVIGATED_EXIT ||
      code == image_editor::ScreenshotCaptureResultCode::USER_ESCAPE_EXIT) {
    RecordCaptureResult(
        code == image_editor::ScreenshotCaptureResultCode::USER_NAVIGATED_EXIT
            ? lens::LensRegionSearchCaptureResult::USER_NAVIGATED_FROM_CAPTURE
            : lens::LensRegionSearchCaptureResult::USER_EXITED_CAPTURE_ESCAPE);
    HandleCaptureFailure();
    return;
  }

  const gfx::Image& image = result.image;
  if (image.IsEmpty()) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::ERROR_CAPTURING_REGION);
    HandleCaptureFailure();
    return;
  }

  // If the capture was successful, proceed with the region search flow.
  HandleCaptureSuccessForSearch(image, result.screen_bounds);
}

void LensRegionSearchController::OnRegionSelectionCompleted(
    const image_editor::RegionSelectionResult& result) {
  if (!is_multi_capture_) {
    // Close all open UI overlays and bubbles.
    CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }

  image_editor::ScreenshotCaptureResultCode code = result.result_code;
  if (code == image_editor::ScreenshotCaptureResultCode::USER_NAVIGATED_EXIT ||
      code == image_editor::ScreenshotCaptureResultCode::USER_ESCAPE_EXIT) {
    RecordCaptureResult(
        code == image_editor::ScreenshotCaptureResultCode::USER_NAVIGATED_EXIT
            ? lens::LensRegionSearchCaptureResult::USER_NAVIGATED_FROM_CAPTURE
            : lens::LensRegionSearchCaptureResult::USER_EXITED_CAPTURE_ESCAPE);
    HandleCaptureFailure();
    return;
  }

  if (result.selected_rect.IsEmpty()) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::ERROR_CAPTURING_REGION);
    HandleCaptureFailure();
    return;
  }

  bounds_callback_.Run(result.selected_rect);
  RecordCaptureResult(lens::LensRegionSearchCaptureResult::SUCCESS);

  if (is_multi_capture_) {
    // Re-start capture flow for another region.
    screenshot_flow_->StartForRegionSelection(base::BindOnce(
        &LensRegionSearchController::OnRegionSelectionCompleted, weak_this_));
  }
}

void LensRegionSearchController::HandleCaptureSuccessForSearch(
    const gfx::Image& image,
    const gfx::Rect& screen_bounds) {
  // Record region size related UMA histograms according to region and screen.
  RecordRegionSizeRelatedMetrics(screen_bounds, image.Size());

  auto* core_tab_helper = CoreTabHelper::FromWebContents(web_contents());
  if (!core_tab_helper) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::FAILED_TO_OPEN_TAB);
    return;
  }

  lens::RecordAmbientSearchQuery(entry_point_);

  if (is_google_default_search_provider_) {
    lens::EntryPoint lens_entry_point =
        lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
    switch (entry_point_) {
      case lens::AmbientSearchEntryPoint::
          LENS_OVERLAY_LOCATION_BAR_ACCESSIBILITY_FALLBACK:
        if (!lens::features::IsLensOverlayKeyboardSelectionEnabled()) {
          lens_entry_point = lens::EntryPoint::CHROME_LENS_OVERLAY_LOCATION_BAR;
        }
        break;
      default:
        // The other possible values of `entry_point_` should not be possible
        // and are considered invalid.
        break;
    }
    core_tab_helper->SearchWithLens(image, lens_entry_point);
  } else {
    core_tab_helper->SearchByImage(image);
  }

  RecordCaptureResult(lens::LensRegionSearchCaptureResult::SUCCESS);
}

void LensRegionSearchController::HandleCaptureFailure() {
  if (region_selection_flow_closed_callback_) {
    std::move(region_selection_flow_closed_callback_).Run();
  }
}

void LensRegionSearchController::WebContentsDestroyed() {
  CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

void LensRegionSearchController::OnVisibilityChanged(
    content::Visibility visibility) {
  if (in_capture_mode_ && visibility == content::Visibility::HIDDEN) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::USER_NAVIGATED_FROM_CAPTURE);
    CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }
}

void LensRegionSearchController::Close() {
  CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  if (region_selection_flow_closed_callback_) {
    std::move(region_selection_flow_closed_callback_).Run();
    return;
  }
  // Record a capture result when the instructional bubble is responsible for
  // exiting out of the capture mode.
  RecordCaptureResult(
      LensRegionSearchCaptureResult::USER_EXITED_CAPTURE_CLOSE_BUTTON);
}

void LensRegionSearchController::Escape() {
  CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  if (region_selection_flow_closed_callback_) {
    std::move(region_selection_flow_closed_callback_).Run();
    return;
  }
  // Record a capture result when the instructional bubble is responsible for
  // exiting out of the capture mode.
  RecordCaptureResult(
      LensRegionSearchCaptureResult::USER_EXITED_CAPTURE_ESCAPE);
}

void LensRegionSearchController::CloseWithReason(
    views::Widget::ClosedReason reason) {
  // Use is_closing_ to prevent re-entrancy. This can be called from
  // OnRegionSelectionCompleted(), which is itself called when the screenshot
  // flow is cancelled via CancelCapture() in this method.
  if (is_closing_) {
    return;
  }
  is_closing_ = true;

  in_capture_mode_ = false;
  if (bubble_widget_) {
    std::exchange(bubble_widget_, nullptr)->CloseWithReason(reason);
  }
  if (screenshot_flow_) {
    screenshot_flow_->CancelCapture();
    screenshot_flow_.reset();
  }
}

bool LensRegionSearchController::IsOverlayUIVisibleForTesting() {
  if (!bubble_widget_ || !screenshot_flow_) {
    return false;
  }
  return bubble_widget_->IsVisible() && screenshot_flow_->IsCaptureModeActive();
}

void LensRegionSearchController::SetEntryPointForTesting(
    lens::AmbientSearchEntryPoint entry_point) {
  entry_point_ = entry_point;
}

void LensRegionSearchController::SetWebContentsForTesting(
    content::WebContents* web_contents) {
  Observe(web_contents);
}

}  // namespace lens
