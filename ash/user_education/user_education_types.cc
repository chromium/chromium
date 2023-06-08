// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_types.h"

namespace ash {

// HelpBubbleMetadata ----------------------------------------------------------

HelpBubbleMetadata::HelpBubbleMetadata(const HelpBubbleKey key,
                                       const aura::Window* anchor_root_window,
                                       const gfx::Rect& anchor_bounds_in_screen)
    : key(key),
      anchor_root_window(anchor_root_window),
      anchor_bounds_in_screen(anchor_bounds_in_screen) {}

HelpBubbleMetadata::~HelpBubbleMetadata() = default;

}  // namespace ash
