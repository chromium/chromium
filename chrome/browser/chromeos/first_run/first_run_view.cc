// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run_view.h"

#include <memory>

#include "chrome/browser/chromeos/first_run/first_run_controller.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_ui.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "url/gurl.h"

namespace chromeos {

FirstRunView::FirstRunView() {}

void FirstRunView::Init(content::BrowserContext* context,
                        FirstRunController* controller) {
  DCHECK(context);
  DCHECK(controller);
  controller_ = controller;
  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_ = AddChildView(std::make_unique<views::WebView>(context));
  web_view_->LoadInitialURL(GURL(chrome::kChromeUIFirstRunURL));

  content::WebContents* web_contents = web_view_->web_contents();
  web_contents->SetDelegate(this);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);

  web_contents->GetRenderViewHost()->GetWidget()->GetView()->SetBackgroundColor(
      SK_ColorTRANSPARENT);
}

FirstRunActor* FirstRunView::GetActor() {
  return static_cast<FirstRunUI*>(
      web_view_->web_contents()->GetWebUI()->GetController())->get_actor();
}

void FirstRunView::RequestFocus() {
  web_view_->RequestFocus();
}

content::WebContents* FirstRunView::GetWebContents() {
  return web_view_->web_contents();
}

bool FirstRunView::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Discards context menu.
  return true;
}

content::KeyboardEventProcessingResult FirstRunView::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == ui::VKEY_ESCAPE) {
    controller_->Cancel();
    return content::KeyboardEventProcessingResult::HANDLED;
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool FirstRunView::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

}  // namespace chromeos
