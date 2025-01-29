// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/webui_contents_container.h"

#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

namespace glic {

WebUIContentsContainer::WebUIContentsContainer(
    Profile* profile,
    GlicWindowController* glic_window_controller)
    : profile_keep_alive_(profile, ProfileKeepAliveOrigin::kGlicView),
      web_contents_(content::WebContents::Create(
          content::WebContents::CreateParams(profile))),
      glic_window_controller_(glic_window_controller) {
  DCHECK(web_contents_);
  web_contents_->SetDelegate(this);
  web_contents_->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL{chrome::kChromeUIGlicURL}));
}

WebUIContentsContainer::~WebUIContentsContainer() {
  web_contents_->ClosePage();
}

bool WebUIContentsContainer::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  GlicView* glic_view = glic_window_controller_->GetGlicView();
  if (!glic_view) {
    return false;
  }
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, glic_view->web_view()->GetFocusManager());
}

void WebUIContentsContainer::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr);
}

}  // namespace glic
