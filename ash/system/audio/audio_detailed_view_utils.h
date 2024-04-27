// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_UTILS_H_
#define ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_UTILS_H_

#include <map>

#include "ash/ash_export.h"
#include "ui/gfx/geometry/insets.h"

namespace views {
class View;
}  // namespace views

namespace ash {
struct AudioDevice;
class HoverHighlightView;

using AudioDeviceViewMap = std::map<views::View*, AudioDevice>;

inline constexpr auto kSubsectionMargins = gfx::Insets::TLBR(0, 0, 4, 0);

// Updates the label and checkmark color of `device_name_view` based on
// whether this device is muted or not.
void UpdateDeviceContainerColor(HoverHighlightView* device_name_view,
                                bool is_muted,
                                bool is_active);

// If an active node's view exists in the `device_map`, updates its color to
// reflect the mute state. Otherwise, takes no action.
void MaybeUpdateActiveDeviceColor(bool is_input,
                                  bool is_muted,
                                  AudioDeviceViewMap& device_map);

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_UTILS_H_
