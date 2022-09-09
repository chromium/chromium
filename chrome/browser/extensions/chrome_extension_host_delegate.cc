// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_host_delegate.h"

#include <memory>
#include <string>

#include "chrome/browser/apps/platform_apps/audio_focus_web_contents_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

ChromeExtensionHostDelegate::ChromeExtensionHostDelegate() {}

ChromeExtensionHostDelegate::~ChromeExtensionHostDelegate() {}

void ChromeExtensionHostDelegate::OnExtensionHostCreated(
    content::WebContents* web_contents) {
  PrefsTabHelper::CreateForWebContents(web_contents);
  apps::AudioFocusWebContentsObserver::CreateForWebContents(web_contents);
}

void ChromeExtensionHostDelegate::OnMainFrameCreatedForBackgroundPage(
    ExtensionHost* host) {
  ExtensionService* service =
      ExtensionSystem::Get(host->browser_context())->extension_service();
  if (service)
    service->DidCreateMainFrameForBackgroundPage(host);
}

content::JavaScriptDialogManager*
ChromeExtensionHostDelegate::GetJavaScriptDialogManager() {
  return javascript_dialogs::AppModalDialogManager::GetInstance();
}

void ChromeExtensionHostDelegate::CreateTab(
    std::unique_ptr<content::WebContents> web_contents,
    const std::string& extension_id,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture) {
  // Verify that the browser is not shutting down. It can be the case if the
  // call is propagated through a posted task that was already in the queue when
  // shutdown started. See crbug.com/625646
  if (g_browser_process->IsShuttingDown())
    return;

  ExtensionTabUtil::CreateTab(std::move(web_contents), extension_id,
                              disposition, initial_rect, user_gesture);
}

void ChromeExtensionHostDelegate::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const Extension* extension) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), extension);
}

bool ChromeExtensionHostDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const Extension* extension) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type,
                                   extension);
}

content::PictureInPictureResult
ChromeExtensionHostDelegate::EnterPictureInPicture(
    content::WebContents* web_contents) {
  return PictureInPictureWindowManager::GetInstance()
      ->EnterVideoPictureInPicture(web_contents);
}

void ChromeExtensionHostDelegate::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

}  // namespace extensions
