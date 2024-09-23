// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/window_icon_util.h"

#include "content/public/browser/desktop_media_id.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_utils.h"

gfx::ImageSkia GetWindowIcon(content::DesktopMediaID id) {
  // TODO(crbug.com/40135428): Hook up Window Icons for lacros.
  DCHECK_EQ(content::DesktopMediaID::TYPE_WINDOW, id.type);
  if (auto* platform_utils =
          ui::OzonePlatform::GetInstance()->GetPlatformUtils()) {
    return platform_utils->GetNativeWindowIcon(id.id);
  }
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::ImageSkia();
}
