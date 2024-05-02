// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_OUTPUT_AUDIO_SLIDERS_VIEW_H_
#define ASH_SYSTEM_AUDIO_OUTPUT_AUDIO_SLIDERS_VIEW_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/system/audio/audio_detailed_view_utils.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace ash {

// This list view displays all currently available output sliders. It is used
// within the `MediaCastAudioSelectorView`.
class ASH_EXPORT OutputAudioSlidersView
    : public CrasAudioHandler::AudioObserver,
      public TrayDetailedView {
  METADATA_HEADER(OutputAudioSlidersView, TrayDetailedView)

 public:
  explicit OutputAudioSlidersView(
      base::RepeatingCallback<void(/*has_devices=*/bool)>
          on_devices_updated_callback);

  OutputAudioSlidersView(const OutputAudioSlidersView&) = delete;
  OutputAudioSlidersView& operator=(const OutputAudioSlidersView&) = delete;
  ~OutputAudioSlidersView() override;

  // Testing methods.
  views::View* GetSliderContainerForTesting() { return slider_container_; }
  AudioDeviceViewMap GetMapForTesting() {
    return output_devices_by_name_views_;
  }

 private:
  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  // CrasAudioHandler::AudioObserver:
  void OnActiveOutputNodeChanged() override;
  void OnAudioNodesChanged() override;
  void OnOutputMuteChanged(bool mute_on) override;

  // Updates with the current output devices.
  void Update();

  // Output audio sliders container.
  raw_ptr<views::View> slider_container_ = nullptr;

  // Runs on devices updated.
  const base::RepeatingCallback<void(/*has_devices=*/bool)>
      on_devices_updated_callback_;

  UnifiedVolumeSliderController unified_volume_slider_controller_;

  AudioDeviceViewMap output_devices_by_name_views_;

  std::optional<uint64_t> focused_device_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_OUTPUT_AUDIO_SLIDERS_VIEW_H_
