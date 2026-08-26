// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller_views.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ttc {

AiOverlayDialogControllerViews::AiOverlayDialogControllerViews(
    BrowserWindowInterface* browser)
    : AiOverlayDialogController(browser) {}

AiOverlayDialogControllerViews::~AiOverlayDialogControllerViews() = default;

views::WebView* AiOverlayDialogControllerViews::GetActiveOverlayWebView()
    const {
  auto* elements = BrowserElementsViews::From(browser());
  if (!elements) {
    return nullptr;
  }
  return elements->GetViewAs<views::WebView>(kAiOverlayDialogWebViewElementId);
}


void AiOverlayDialogControllerViews::ShowOverlay() {
  views::WebView* overlay_web_view = GetActiveOverlayWebView();
  if (!overlay_web_view) {
    return;
  }

  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      overlay_web_view->GetWebContents(), SK_ColorTRANSPARENT);

  webui::SetBrowserWindowInterface(overlay_web_view->GetWebContents(),
                                   browser());
  extensions::SetViewType(overlay_web_view->GetWebContents(),
                          extensions::mojom::ViewType::kComponent);
  overlay_web_view->GetWebContents()->SetDelegate(this);

  overlay_web_view->LoadInitialURL(
      GURL(chrome::kChromeUIAiOverlayDialogUntrustedURL));

  overlay_web_view->SetVisible(true);
  overlay_web_view->InvalidateLayout();
  overlay_web_view->parent()->InvalidateLayout();

  if (overlay_web_view->GetWidget()) {
    overlay_web_view->GetWidget()->LayoutRootViewIfNecessary();
  }

  if (auto* action_item = actions::ActionManager::Get().FindAction(
          kActionShowAiOverlayDialog,
          browser()->GetFeatures().GetRootActionItem())) {
    action_item->SetImage(ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? vector_icons::kPauseFilledIcon
                                          : vector_icons::kPauseOldIcon,
        ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize));
    action_item->SetProperty(kActionAiOverlayActiveKey, true);
  }

  // Update the action state to ensure the toolbar button prevents overflow when
  // the dialog is active.
  if (auto* pinned_actions =
          browser()->GetFeatures().pinned_toolbar_actions()) {
    pinned_actions->UpdateActionState(kActionShowAiOverlayDialog,
                                      /*is_active=*/true);
  }
}

void AiOverlayDialogControllerViews::HideOverlay() {
  views::WebView* overlay_web_view = GetActiveOverlayWebView();
  if (overlay_web_view) {
    overlay_web_view->SetVisible(false);
  }

  if (auto* action_item = actions::ActionManager::Get().FindAction(
          kActionShowAiOverlayDialog,
          browser()->GetFeatures().GetRootActionItem())) {
    action_item->SetImage(ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? vector_icons::kMicFilledIcon
                                          : vector_icons::kMicOldIcon,
        ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize));
    action_item->SetProperty(kActionAiOverlayActiveKey, false);
  }

  // Update the action state to ensure the toolbar button prevents overflow when
  // the dialog is active.
  if (auto* pinned_actions =
          browser()->GetFeatures().pinned_toolbar_actions()) {
    pinned_actions->UpdateActionState(kActionShowAiOverlayDialog,
                                      /*is_active=*/false);
  }
}

bool AiOverlayDialogControllerViews::IsOverlayShowing() const {
  views::WebView* overlay_web_view = GetActiveOverlayWebView();
  return overlay_web_view != nullptr && overlay_web_view->GetVisible();
}

void AiOverlayDialogControllerViews::ResizeDueToAutoResize(
    content::WebContents* source,
    const gfx::Size& new_size) {
  views::WebView* overlay_web_view = GetActiveOverlayWebView();
  if (overlay_web_view && overlay_web_view->GetWebContents() == source) {
    overlay_web_view->SetPreferredSize(new_size);
  }
}

}  // namespace ttc
