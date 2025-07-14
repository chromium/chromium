// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_host_delegate.h"

#include <memory>
#include <string>

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
