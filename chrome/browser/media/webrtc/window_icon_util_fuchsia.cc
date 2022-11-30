// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/window_icon_util.h"

#include "base/notreached.h"
#include "content/public/browser/desktop_media_id.h"
#include "ui/gfx/image/image_skia.h"

gfx::ImageSkia GetWindowIcon(content::DesktopMediaID id) {
  // TODO(crbug.com/1234750)
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::ImageSkia();
}
