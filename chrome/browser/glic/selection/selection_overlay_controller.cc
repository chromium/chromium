// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_controller.h"

#include "base/strings/to_string.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/skia_conversions.h"

// TODO(http://b/485358530): Consider `OverlayBaseController::State` to the
// mojom file so the << operator is auto generated.
std::ostream& operator<<(std::ostream& os, OverlayBaseController::State value) {
  switch (value) {
    case OverlayBaseController::State::kOff:
      return os << "kOff";
    case OverlayBaseController::State::kClosingOpenedSidePanel:
      return os << "kClosingOpenedSidePanel";
    case OverlayBaseController::State::kScreenshot:
      return os << "kScreenshot";
    case OverlayBaseController::State::kStartingWebUI:
      return os << "kStartingWebUI";
    case OverlayBaseController::State::kOverlay:
      return os << "kOverlay";
    case OverlayBaseController::State::kHidden:
      return os << "kHidden";
    case OverlayBaseController::State::kBackground:
      return os << "kBackground";
    case OverlayBaseController::State::kClosing:
      return os << "kClosing";
    case OverlayBaseController::State::kIsReshowing:
      return os << "kIsReshowing";
    case OverlayBaseController::State::kHiding:
      return os << "kHiding";
  }
}

namespace glic {

DEFINE_USER_DATA(SelectionOverlayController);

SelectionOverlayController::SelectionOverlayController(
    tabs::TabInterface* tab,
    PrefService* pref_service)
    : OverlayBaseController(tab, pref_service),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

SelectionOverlayController::~SelectionOverlayController() = default;

// static.
SelectionOverlayController* SelectionOverlayController::FromOverlayWebContents(
    content::WebContents* overlay_web_contents) {
  return Get(
      webui::GetTabInterface(overlay_web_contents)->GetUnownedUserDataHost());
}

// static.
SelectionOverlayController* SelectionOverlayController::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  return Get(tabs::TabInterface::GetFromContents(tab_web_contents)
                 ->GetUnownedUserDataHost());
}

void SelectionOverlayController::BindOverlay(
    mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
    mojo::PendingRemote<selection::SelectionOverlayPage> page) {
  CHECK_EQ(state(), State::kStartingWebUI) << base::ToString(state());

  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &SelectionOverlayController::Reset, weak_factory_.GetWeakPtr()));
  page_.Bind(std::move(page));

  InitializeOverlay();
}

void SelectionOverlayController::Show() {
  ShowModalUI();
}

void SelectionOverlayController::Hide() {
  HideOverlay();
}

void SelectionOverlayController::RequestSyncClose(
    DismissalSource dismissal_source) {
  CloseUI();
}

void SelectionOverlayController::InitializeOverlay() {
  // We can only continue once both the WebUI is bound and the initialization
  // data is processed and ready. If either of those conditions aren't met, we
  // exit early and wait for the other condition to call this method again.
  if (!page_ || !screenshot_available_) {
    return;
  }

  InitializeOverlayImpl();

  CHECK(page_);
  page_->ScreenshotReceived(initial_rgb_screenshot_);
}

void SelectionOverlayController::StartScreenshotFlow() {
  content::RenderWidgetHostView* view = tab_->GetContents()
                                            ->GetPrimaryMainFrame()
                                            ->GetRenderViewHost()
                                            ->GetWidget()
                                            ->GetView();
  // Side panel is now fully closed, take screenshot and open overlay.
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(), base::TimeDelta(),
      base::BindOnce(&SelectionOverlayController::OnScreenshotTaken,
                     weak_factory_.GetWeakPtr()));
}

void SelectionOverlayController::NotifyOverlayClosing() {}

void SelectionOverlayController::OnScreenshotTaken(
    const content::CopyFromSurfaceResult& result) {
  const SkBitmap& bitmap = result.has_value() ? result->bitmap : SkBitmap();
  InitializeScreenshot(
      bitmap, base::BindOnce(&SelectionOverlayController::SetScreenshot,
                             weak_factory_.GetWeakPtr(), bitmap));
}

void SelectionOverlayController::SetScreenshot(const SkBitmap& screenshot,
                                               SkBitmap rgb_screenshot) {
  initial_screenshot_ = screenshot;
  initial_rgb_screenshot_ = std::move(rgb_screenshot);
  screenshot_available_ = true;
  InitializeOverlay();
}

bool SelectionOverlayController::IsResultsSidePanelShowing() {
  return true;
}

GURL SelectionOverlayController::GetInitialURL() {
  return GURL(chrome::kChromeUIGlicSelectionOverlayURL);
}

void SelectionOverlayController::NotifyIsOverlayShowing(bool is_showing) {}

int SelectionOverlayController::GetToolResourceId() {
  return IDS_GLIC_SELECTION_OVERLAY_RENDERER_LABEL;
}

ui::ElementIdentifier SelectionOverlayController::GetViewContainerId() {
  return kGlicSelectionOverlayViewElementId;
}

SidePanelEntry::PanelType SelectionOverlayController::GetSidePanelType() {
  return SidePanelEntry::PanelType::kContent;
}

bool SelectionOverlayController::ShouldCloseSidePanel() {
  return false;
}

bool SelectionOverlayController::ShouldShowPreselectionBubble() {
  return true;
}

bool SelectionOverlayController::UseOverlayBlur() {
  return true;
}

void SelectionOverlayController::NotifyPageNavigated() {}

void SelectionOverlayController::NotifyTabForegrounded() {}

void SelectionOverlayController::NotifyTabWillEnterBackground() {}

bool SelectionOverlayController::IsOverlayViewShared() const {
  // Glic's selection overlay's WebView is attached to the ContentsContainerView
  // which cannot be shared across multiple tabs.
  return false;
}

void SelectionOverlayController::DismissOverlay(
    selection::DismissOverlayReason reason) {
  CloseUI();
}

void SelectionOverlayController::AdjustRegion(
    selection::SelectedRegionPtr target) {
  auto it = selected_regions_.find(target->id);
  if (it != selected_regions_.end()) {
    it->second = std::move(target);
  } else {
    selected_regions_[target->id] = std::move(target);
  }

  RenderRegions();
}

void SelectionOverlayController::DeleteRegion(
    const base::UnguessableToken& id) {
  if (selected_regions_.erase(id)) {
    RenderRegions();
  }
}

void SelectionOverlayController::Reset() {
  receiver_.reset();
  page_.reset();
  initial_rgb_screenshot_.reset();
  initial_screenshot_.reset();
  screenshot_available_ = false;
  encoded_.reset();
  selected_regions_.clear();
}

void SelectionOverlayController::RenderRegions() {
  if (initial_screenshot_.empty()) {
    return;
  }

  SkBitmap deep_copy_bitmap;
  // TODO(http://b/485358530): Record proper histograms for the error case.
  // Allocate memory for the deep copy.
  if (!deep_copy_bitmap.tryAllocPixels(initial_screenshot_.info())) {
    LOG(ERROR) << "Alloc failure";
    return;
  }

  SkCanvas canvas(deep_copy_bitmap);
  canvas.drawImage(initial_screenshot_.asImage(), 0, 0);
  SkPaint paint;
  const SkScalar intervals[] = {5.0f, 5.0f};
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(2.0f);

  // TODO(http://b/452032491): Reconsider what happens if the regions overlap.
  // TODO(http://b/452032491): Currently this class is only used once per
  // selection and only one region is supported, so it is fine to always loop
  // through all the regions. Revisit once we expand the selections.
  for (const auto& [id, region] : selected_regions_) {
    SkRect rect_on_canvas = gfx::RectFToSkRect(region->region);
    if (!rect_on_canvas.isEmpty() &&
        initial_screenshot_.bounds().contains(rect_on_canvas)) {
      paint.setColor(SK_ColorMAGENTA);
      paint.setPathEffect(SkDashPathEffect::Make(intervals, 0.0f));
      canvas.drawRect(rect_on_canvas, paint);
      paint.setPathEffect(SkDashPathEffect::Make(intervals, -5.0f));
      paint.setColor(SK_ColorCYAN);
      canvas.drawRect(rect_on_canvas, paint);
    } else {
      // TODO(http://b/485358530): Record proper histograms for the error case.
      LOG(ERROR) << "Invalid region selected " << region->region.ToString();
    }
  }

  encoded_ = gfx::JPEGCodec::Encode(deep_copy_bitmap, 40);
}

}  // namespace glic
