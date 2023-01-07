// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_topic_queue_slideshow_delegate.h"

#include <algorithm>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

AmbientTopicQueueSlideshowDelegate::AmbientTopicQueueSlideshowDelegate() =
    default;

AmbientTopicQueueSlideshowDelegate::~AmbientTopicQueueSlideshowDelegate() =
    default;

std::vector<gfx::Size> AmbientTopicQueueSlideshowDelegate::GetTopicSizes() {
  auto* ambient_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AmbientModeContainer);
  gfx::Size display_size_px = display::Screen::GetScreen()
                                  ->GetDisplayNearestView(ambient_container)
                                  .GetSizeInPixel();

  // For portrait photos, the server returns image of half requested width.
  // When the device is in portrait mode, where only shows one portrait photo,
  // it will cause unnecessary scaling. To reduce this effect, always requesting
  // the landscape display size.
  const int width = std::max(display_size_px.width(), display_size_px.height());
  const int height =
      std::min(display_size_px.width(), display_size_px.height());
  return {gfx::Size(width, height)};
}

}  // namespace ash
