// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WINDOW_ICON_UTIL_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WINDOW_ICON_UTIL_H_

#include "content/public/browser/desktop_media_id.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "ui/gfx/image/image_skia.h"

gfx::ImageSkia GetWindowIcon(content::DesktopMediaID id);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WINDOW_ICON_UTIL_H_
