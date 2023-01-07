// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/window_icon_util.h"

#include "content/public/browser/desktop_media_id.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

gfx::ImageSkia GetWindowIcon(content::DesktopMediaID id) {
  DCHECK_EQ(content::DesktopMediaID::TYPE_WINDOW, id.type);
  aura::Window* window = content::DesktopMediaID::GetNativeWindowById(id);
  if (!window)
    return gfx::ImageSkia();

  gfx::ImageSkia* image = window->GetProperty(aura::client::kWindowIconKey);
  if (!image)
    image = window->GetProperty(aura::client::kAppIconKey);
  return image ? *image : gfx::ImageSkia();
}
