// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_desktop_media_id.h"

#import <Cocoa/Cocoa.h>

#include "components/remote_cocoa/browser/scoped_cg_window_id.h"

std::optional<content::DesktopMediaID> GetDesktopMediaIDForBrowserWindow(
    gfx::NativeWindow window) {
  NSWindow* ns_window = window.GetNativeNSWindow();
  if (!ns_window || ns_window.windowNumber <= 0) {
    return std::nullopt;
  }
  const content::DesktopMediaID::Id id = ns_window.windowNumber;
  // Like the picker in NativeDesktopMediaList: set `window_id`, so that the
  // launcher uses ViewsWidgetVideoCaptureDeviceMac (viz) instead of an OS
  // capturer. This needs a CGWindowID that remote_cocoa knows.
  if (!remote_cocoa::ScopedCGWindowID::Get(id)) {
    return std::nullopt;
  }
  content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_WINDOW, id);
  media_id.window_id = id;
  return media_id;
}
