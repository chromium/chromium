// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_host_delegate.h"

#include <memory>
#include <string>

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "chrome/browser/apps/platform_apps/audio_focus_web_contents_observer.h"
#endif

// This file contains code shared between Android and non-Android platforms.
static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ChromeExtensionHostDelegate::ChromeExtensionHostDelegate() = default;

ChromeExtensionHostDelegate::~ChromeExtensionHostDelegate() = default;

void ChromeExtensionHostDelegate::OnExtensionHostCreated(
    content::WebContents* web_contents) {
  PrefsTabHelper::CreateForWebContents(web_contents);
#if BUILDFLAG(ENABLE_PLATFORM_APPS)
  apps::AudioFocusWebContentsObserver::CreateForWebContents(web_contents);
#endif
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
    const url::Origin& security_origin,
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
