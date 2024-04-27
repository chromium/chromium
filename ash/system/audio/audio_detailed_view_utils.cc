// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_detailed_view_utils.h"

#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace ash {

void UpdateDeviceContainerColor(HoverHighlightView* device_name_view,
                                bool is_muted,
                                bool is_active) {
  const ui::ColorId color_id =
      is_active ? (is_muted ? cros_tokens::kCrosSysOnSurface
                            : cros_tokens::kCrosSysSystemOnPrimaryContainer)
                : cros_tokens::kCrosSysOnSurfaceVariant;
  device_name_view->text_label()->SetEnabledColorId(color_id);
  TrayPopupUtils::UpdateCheckMarkColor(device_name_view, color_id);
}

void MaybeUpdateActiveDeviceColor(bool is_input,
                                  bool is_muted,
                                  AudioDeviceViewMap& device_map) {
  // Only the active node could trigger the mute state change. Iterates the
  // `device_map_` to find the corresponding `device_name_view` and
  // updates the color.
  auto it = base::ranges::find(
      device_map, CrasAudioHandler::Get()->GetPrimaryActiveOutputNode(),
      [](const AudioDeviceViewMap::value_type& value) {
        return value.second.id;
      });

  if (it != device_map.end()) {
    UpdateDeviceContainerColor(
        views::AsViewClass<HoverHighlightView>(it->first), is_muted,
        it->second.active);
  }
}

}  // namespace ash
