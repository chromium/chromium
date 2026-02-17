// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/views/view_class_properties.h"

namespace contextual_tasks {

ContextualTasksWebView::ContextualTasksWebView(
    content::BrowserContext* browser_context)
    : views::WebView(browser_context) {
  SetProperty(views::kElementIdentifierKey,
              kContextualTasksSidePanelWebViewElementId);
}

ContextualTasksWebView::~ContextualTasksWebView() {
  SetWebContents(nullptr);
}

base::WeakPtr<ContextualTasksWebView> ContextualTasksWebView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ContextualTasksWebView::SetWebContents(content::WebContents* wc) {
  if (web_contents() == wc) {
    return;
  }

  if (web_contents() && !web_contents()->IsBeingDestroyed()) {
    web_contents()->WasHidden();
  }
  DetachWebContentsModalDialogManager(web_contents());

  AttachWebContentsModalDialogManager(wc);
  views::WebView::SetWebContents(wc);

  if (wc) {
    wc->WasShown();
    // Set `this` as the delegate to handle media access permissions.
    wc->SetDelegate(this);
    // Set ViewType::kComponent so `ChromeSpeechRecognitionManagerDelegate`
    // allows speech recognition in `CheckRenderFrameType()`.
    extensions::SetViewType(wc, extensions::mojom::ViewType::kComponent);
  }
}

void ContextualTasksWebView::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Handle the media access requests for voice search by routing them through
  // `MediaCaptureDevicesDispatcher`.
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

bool ContextualTasksWebView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

content::WebContents* ContextualTasksWebView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  BrowserWindowInterface* browser = GetBrowser();
  if (browser) {
    return browser->OpenURL(params, std::move(navigation_handle_callback));
  } else {
    VLOG(1) << "Cannot find browser to open URL from tab.";
    return nullptr;
  }
}

web_modal::WebContentsModalDialogHost*
ContextualTasksWebView::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  if (!GetBrowser()) {
    return nullptr;
  }
  return GetBrowser()->GetWebContentsModalDialogHostForWindow();
}

void ContextualTasksWebView::AttachWebContentsModalDialogManager(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
}

void ContextualTasksWebView::DetachWebContentsModalDialogManager(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  auto* dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (dialog_manager) {
    dialog_manager->SetDelegate(nullptr);
  }
}

BrowserWindowInterface* ContextualTasksWebView::GetBrowser() {
  if (!web_contents()) {
    return nullptr;
  }
  return webui::GetBrowserWindowInterface(web_contents());
}

}  // namespace contextual_tasks
