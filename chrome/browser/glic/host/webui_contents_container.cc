// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/webui_contents_container.h"

#include "base/check.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/webview/webview.h"

namespace glic {

WebUIContentsContainer::WebUIContentsContainer(
    Profile* profile,
    GlicWindowController* glic_window_controller)
    : profile_keep_alive_(profile, ProfileKeepAliveOrigin::kGlicView),
      web_contents_(content::WebContents::Create(
          content::WebContents::CreateParams(profile))),
      glic_window_controller_(glic_window_controller) {
  CHECK(web_contents_);
  Observe(web_contents_.get());
  web_contents_->SetDelegate(this);
  web_contents_->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL{chrome::kChromeUIGlicURL}));
}

WebUIContentsContainer::~WebUIContentsContainer() {
  web_contents_->ClosePage();
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (!glic_profile_manager) {
    return;
  }
  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      glic_window_controller_->profile());
  glic_profile_manager->OnUnloadingClientForService(glic_service);
}

bool WebUIContentsContainer::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  GlicView* glic_view = glic_window_controller_->GetGlicView();
  return glic_view && unhandled_keyboard_event_handler_.HandleKeyboardEvent(
                          event, glic_view->GetFocusManager());
}

void WebUIContentsContainer::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr);
}

void WebUIContentsContainer::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    base::RecordAction(base::UserMetricsAction("GlicSessionWebUiCrash"));
  }
  auto* keyed_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      web_contents_->GetBrowserContext());
  keyed_service->CloseUI();
  // WARNING: Do not do any more work, as `this` may have been destroyed.
}

}  // namespace glic
