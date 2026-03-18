// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"

#include "base/functional/callback_forward.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

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
  if (auto* rwhv =
          overlay_web_view->GetWebContents()->GetRenderWidgetHostView()) {
    rwhv->SetBackgroundColor(SK_ColorTRANSPARENT);
  }

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
}

void AiOverlayDialogController::HideOverlay() {
  views::WebView* overlay_web_view = GetActiveOverlayWebView();
  if (overlay_web_view) {
    overlay_web_view->SetVisible(false);
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
