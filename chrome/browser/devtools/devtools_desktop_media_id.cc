// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_desktop_media_id.h"

#include "build/build_config.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

std::optional<content::DesktopMediaID> GetDesktopMediaIDForBrowserWindow(
    gfx::NativeWindow window) {
#if defined(USE_AURA)
  if (!window) {
    return std::nullopt;
  }
  // Like the picker in NativeDesktopMediaList: register the aura::Window, so
  // that the launcher uses AuraWindowVideoCaptureDevice.
  return content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_WINDOW, window);
#else
  return std::nullopt;
#endif
}
