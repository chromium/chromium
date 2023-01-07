// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_VIEW_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_VIEW_H_

#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/unified/unified_slider_view.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

// View of a slider that can change audio volume.
class UnifiedVolumeView : public UnifiedSliderView,
                          public CrasAudioHandler::AudioObserver {
 public:
  UnifiedVolumeView(UnifiedVolumeSliderController* controller,
                    UnifiedVolumeSliderController::Delegate* delegate);

  UnifiedVolumeView(const UnifiedVolumeView&) = delete;
  UnifiedVolumeView& operator=(const UnifiedVolumeView&) = delete;

  ~UnifiedVolumeView() override;

  // views::View:
  const char* GetClassName() const override;

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

  views::Button* const more_button_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_VIEW_H_
