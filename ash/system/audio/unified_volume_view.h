// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_VIEW_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_VIEW_H_

#include "ash/ash_export.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/unified/unified_slider_view.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {
class IconButton;

// View of a slider that can change audio volume.
class ASH_EXPORT UnifiedVolumeView : public UnifiedSliderView,
                                     public CrasAudioHandler::AudioObserver {
 public:
  METADATA_HEADER(UnifiedVolumeView);

  UnifiedVolumeView(UnifiedVolumeSliderController* controller,
                    UnifiedVolumeSliderController::Delegate* delegate);

  UnifiedVolumeView(const UnifiedVolumeView&) = delete;
  UnifiedVolumeView& operator=(const UnifiedVolumeView&) = delete;

  ~UnifiedVolumeView() override;

  // References to the icons that correspond to different volume levels used in
  // the `QuickSettingsSlider`. Defined as a public member to be used in tests.
  static constexpr const gfx::VectorIcon* kQsVolumeLevelIcons[] = {
      &kUnifiedMenuVolumeMuteIcon,    // Mute volume.
      &kUnifiedMenuVolumeMediumIcon,  // Medium volume.
      &kUnifiedMenuVolumeHighIcon,    // High volume.
  };

 private:
  void Update(bool by_user);

  // CrasAudioHandler::AudioObserver:
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on) override;
  void OnAudioNodesChanged() override;
  void OnActiveOutputNodeChanged() override;
  void OnActiveInputNodeChanged() override;

  // UnifiedSliderView:
  void ChildVisibilityChanged(views::View* child) override;

  IconButton* const more_button_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_VIEW_H_
