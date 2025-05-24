// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_UTILS_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_UTILS_H_

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "ui/gfx/image/image_skia.h"

gfx::ImageSkia ScaleBitmap(const SkBitmap& bitmap, gfx::Size size);

// Convert between content DesktopMediaID::Type and DesktopMediaList::Type.
// (Note that these functions are not mutual inverses.)
content::DesktopMediaID::Type AsDesktopMediaIdType(DesktopMediaList::Type type);
DesktopMediaList::Type AsDesktopMediaListType(
    content::DesktopMediaID::Type type);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_UTILS_H_
