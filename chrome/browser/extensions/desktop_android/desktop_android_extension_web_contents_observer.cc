// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_extension_web_contents_observer.h"

namespace extensions {

DesktopAndroidExtensionWebContentsObserver::
    DesktopAndroidExtensionWebContentsObserver(
        content::WebContents* web_contents)
    : ExtensionWebContentsObserver(web_contents),
      content::WebContentsUserData<DesktopAndroidExtensionWebContentsObserver>(
          *web_contents) {}

DesktopAndroidExtensionWebContentsObserver::
    ~DesktopAndroidExtensionWebContentsObserver() = default;

void DesktopAndroidExtensionWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  content::WebContentsUserData<DesktopAndroidExtensionWebContentsObserver>::
      CreateForWebContents(web_contents);

  // Initialize this instance if necessary.
  FromWebContents(web_contents)->Initialize();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DesktopAndroidExtensionWebContentsObserver);

}  // namespace extensions
