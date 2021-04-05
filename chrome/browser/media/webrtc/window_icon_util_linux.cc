// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/window_icon_util.h"

#include "content/public/browser/desktop_media_id.h"
#include "ui/aura/client/aura_constants.h"

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_utils.h"
#endif
#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

gfx::ImageSkia GetWindowIcon(content::DesktopMediaID id) {
  // TODO(https://crbug.com/1094460): Hook up Window Icons for lacros.
  DCHECK_EQ(content::DesktopMediaID::TYPE_WINDOW, id.type);
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    if (auto* platform_utils =
            ui::OzonePlatform::GetInstance()->GetPlatformUtils()) {
      return platform_utils->GetNativeWindowIcon(id.id);
    }
    NOTIMPLEMENTED_LOG_ONCE();
    return gfx::ImageSkia();
  }
#endif
#if defined(USE_X11)
  return ui::GetNativeWindowIcon(id.id);
#endif
  NOTREACHED();
  return gfx::ImageSkia();
}
