// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_VIEW_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_VIEW_H_

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/quick_settings_slider.h"
#include "ash/system/unified/unified_slider_view.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {
class IconButton;
class UnifiedVolumeSliderController;

// View of a slider that can change audio volume.
class ASH_EXPORT UnifiedVolumeView : public UnifiedSliderView,
                                     public AccessibilityObserver,
                                     public CrasAudioHandler::AudioObserver {
  METADATA_HEADER(UnifiedVolumeView, UnifiedSliderView)

 public:
  // This constructor is to create the `UnifiedVolumeView` with a trailing
  // settings button that leads to `AudioDetailedView`, i.e. volume slider in
  // the Quick Settings main page and Quick Settings toasts. `delegate` is used
  // to construct the callback for `more_button_`. The displayed slider will
  // always be the active output node, so `device_id` is not needed.
  UnifiedVolumeView(UnifiedVolumeSliderController* controller,
                    UnifiedVolumeSliderController::Delegate* delegate,
                    bool is_active_output_node);
  // This constructor is for single `UnifiedVolumeView`, i.e. volume sliders in
  // `AudioDetailedView`. `delegate` is not needed since there'll be no trailing
  // `more_button_`.
  UnifiedVolumeView(UnifiedVolumeSliderController* controller,
                    uint64_t device_id,
                    bool is_active_output_node,
                    const gfx::Insets& inside_padding);

  UnifiedVolumeView(const UnifiedVolumeView&) = delete;
  UnifiedVolumeView& operator=(const UnifiedVolumeView&) = delete;

  ~UnifiedVolumeView() override;

  // References to the icons that correspond to different volume levels used in
  // the `QuickSettingsSlider`. Defined as a public member to be used in tests.
  static constexpr const gfx::VectorIcon* kQsVolumeLevelIcons[] = {
      &kUnifiedMenuVolumeMuteIcon,    // Muted.
      &kUnifiedMenuVolumeMediumIcon,  // Medium volume.
      &kUnifiedMenuVolumeHighIcon,    // High volume.
  };

  // The maximum index of `kQsVolumeLevelIcons`.
  static constexpr int kQsVolumeLevels = std::size(kQsVolumeLevelIcons) - 1;

  IconButton* more_button() { return more_button_; }

 private:
  friend class UnifiedVolumeViewTest;

  void Update(bool by_user);

  // Get `VectorIcon` reference that corresponds to the given volume level.
  // `level` is between 0.0 to 1.0 inclusive.
  const gfx::VectorIcon& GetVolumeIconForLevel(float level);

  // Callback called when the `live_caption_button_` is pressed.
  void OnLiveCaptionButtonPressed();

  // CrasAudioHandler::AudioObserver:
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on) override;
  void OnAudioNodesChanged() override;
  void OnActiveOutputNodeChanged() override;
  void OnActiveInputNodeChanged() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // UnifiedSliderView:
  void ChildVisibilityChanged(views::View* child) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  const raw_ptr<IconButton> more_button_;

  // Whether this `UnifiedVolumeView` is the view for the active output node.
  bool const is_active_output_node_;

  uint64_t device_id_ = 0;
  // Owned by the views hierarchy.
  raw_ptr<IconButton> live_caption_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_VIEW_H_
