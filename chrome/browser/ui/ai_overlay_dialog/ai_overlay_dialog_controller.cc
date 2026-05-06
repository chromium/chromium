// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"

#include "base/functional/callback_forward.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ttc {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kActionAiOverlayActiveKey, false)

DEFINE_USER_DATA(AiOverlayDialogController);

// static
AiOverlayDialogController* AiOverlayDialogController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

AiOverlayDialogController::AiOverlayDialogController(
    BrowserWindowInterface* browser)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(browser->GetProfile())) {
  // TODO(crbug.com/502801064): If this is to ever be productionized this isn't
  // the right way to get microphone permission.
  auto url = GURL(chrome::kChromeUIAiOverlayDialogUntrustedURL);
  host_content_settings_map_->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW);
}

AiOverlayDialogController::~AiOverlayDialogController() = default;

views::WebView* AiOverlayDialogController::GetActiveOverlayWebView() const {
  auto* elements = BrowserElementsViews::From(browser_);
  if (!elements) {
    return nullptr;
  }
  return elements->GetViewAs<views::WebView>(kAiOverlayDialogWebViewElementId);
}

void AiOverlayDialogController::ShowOverlay() {
  views::WebView* overlay_web_view = GetActiveOverlayWebView();
  if (!overlay_web_view) {
    return;
  }

  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      overlay_web_view->GetWebContents(), SK_ColorTRANSPARENT);

  webui::SetBrowserWindowInterface(overlay_web_view->GetWebContents(),
                                   browser_);
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
          browser_->GetActions()->root_action_item())) {
    action_item->SetImage(
        ui::ImageModel::FromVectorIcon(vector_icons::kPauseIcon, ui::kColorIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize));
    action_item->SetProperty(kActionAiOverlayActiveKey, true);
  }

  // Update the action state to ensure the toolbar button prevents overflow when
  // the dialog is active.
  if (auto* pinned_actions = browser_->GetFeatures().pinned_toolbar_actions()) {
    pinned_actions->UpdateActionState(kActionShowAiOverlayDialog,
                                      /*is_active=*/true);
  }
}

void AiOverlayDialogController::HideOverlay() {
  views::WebView* overlay_web_view = GetActiveOverlayWebView();
  if (overlay_web_view) {
    overlay_web_view->SetVisible(false);
  }

  if (auto* action_item = actions::ActionManager::Get().FindAction(
          kActionShowAiOverlayDialog,
          browser_->GetActions()->root_action_item())) {
    action_item->SetImage(
        ui::ImageModel::FromVectorIcon(vector_icons::kMicIcon, ui::kColorIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize));
    action_item->SetProperty(kActionAiOverlayActiveKey, false);
  }

  // Update the action state to ensure the toolbar button prevents overflow when
  // the dialog is active.
  if (auto* pinned_actions = browser_->GetFeatures().pinned_toolbar_actions()) {
    pinned_actions->UpdateActionState(kActionShowAiOverlayDialog,
                                      /*is_active=*/false);
  }
}

void AiOverlayDialogController::ToggleOverlay() {
  if (IsOverlayShowing()) {
    HideOverlay();
  } else {
    ShowOverlay();
  }
}

bool AiOverlayDialogController::IsOverlayShowing() const {
  views::WebView* overlay_web_view = GetActiveOverlayWebView();
  return overlay_web_view != nullptr && overlay_web_view->GetVisible();
}

void AiOverlayDialogController::set_captions_visible(bool visible) {
  VLOG(1) << "set_captions_visible: " << visible;
  if (captions_visible_ == visible) {
    return;
  }
  captions_visible_ = visible;
  for (auto& observer : observers_) {
    observer.OnCaptionsVisibleChanged(visible);
  }
}

void AiOverlayDialogController::set_use_persona(bool use_persona) {
  VLOG(1) << "set_use_persona: " << use_persona;
  if (use_persona_ == use_persona) {
    return;
  }
  use_persona_ = use_persona;
  for (auto& observer : observers_) {
    observer.OnUsePersonaChanged(use_persona);
  }
}

void AiOverlayDialogController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AiOverlayDialogController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AiOverlayDialogController::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

bool AiOverlayDialogController::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return true;
}

void AiOverlayDialogController::ResizeDueToAutoResize(
    content::WebContents* source,
    const gfx::Size& new_size) {
  views::WebView* overlay_web_view = GetActiveOverlayWebView();
  if (overlay_web_view && overlay_web_view->GetWebContents() == source) {
    overlay_web_view->SetPreferredSize(new_size);
  }
}

}  // namespace ttc
