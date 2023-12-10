// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_VC_TILE_UI_CONTROLLER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_VC_TILE_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class VcEffectState;
enum class VcEffectId;

namespace video_conference {

// The controller for the UI of toggle tiles in the VC controls bubble. There is
// one controller per tile. Non-UI logic for the tiles is handled by
// `ash::VcEffectsDelegate`.
//
// Note: This class is only used when `VcDlcUi` is enabled.
class ASH_EXPORT VcTileUiController {
 public:
  explicit VcTileUiController(const VcHostedEffect* effect);
  VcTileUiController(const VcTileUiController&) = delete;
  VcTileUiController& operator=(const VcTileUiController&) = delete;
  ~VcTileUiController();

  // Creates and returns the `FeatureTile` associated with this controller.
  std::unique_ptr<FeatureTile> CreateTile();

 private:
  friend class VcTileUiControllerTest;

  // Called when the `FeatureTile` associated with this controller is pressed.
  void OnPressed(const ui::Event& event);

  // Sets the tooltip text based on the associated tile's toggle state.
  void UpdateTooltip();

  // Tracks the toggling behavior. Obtains the histogram name with the help of
  // `GetEffectId()`.
  void TrackToggleUMA(bool target_toggle_state);

  // Plays a haptic effect based on `target_toggle_state`.
  void PlayToggleHaptic(bool target_toggle_state);

  // A weak pointer to the `FeatureTile` associated with this controller. Note
  // that this is guaranteed to be nullptr before `CreateTile()` is called, and
  // may even be nullptr after `CreateTile()` is called (the tile is owned by
  // the Views hierarchy, not this controller).
  base::WeakPtr<FeatureTile> tile_ = nullptr;

  // The effect id which is used for UMA tracking.
  VcEffectId effect_id_;

  // Information about the associated video conferencing effect needed to
  // display the UI of the tile controlled by this controller.
  raw_ptr<const VcEffectState> effect_state_ = nullptr;
  raw_ptr<const VcHostedEffect> effect_ = nullptr;

  base::WeakPtrFactory<VcTileUiController> weak_ptr_factory_{this};
};

}  // namespace video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_VC_TILE_UI_CONTROLLER_H_
