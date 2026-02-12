// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_controller.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_view_host.h"

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
  if (!screenshot_available_) {
    return;
  }

  InitializeOverlayImpl();
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
  // TODO(b:479179977): Switch to glic selection overlay.
  return GURL(chrome::kChromeUILensOverlayUntrustedURL);
}

void SelectionOverlayController::NotifyIsOverlayShowing(bool is_showing) {}

int SelectionOverlayController::GetToolResourceId() {
  // TODO(b:479179977): Switch to glic selection overlay.
  return IDS_LENS_OVERLAY_RENDERER_LABEL;
}

ui::ElementIdentifier SelectionOverlayController::GetViewContainerId() {
  // TODO(b:479179977): Switch to glic selection overlay.
  return kLensOverlayViewElementId;
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

}  // namespace glic
