// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_FAKE_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_FAKE_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/system/video_conference/bubble/vc_tile_ui_controller.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"

namespace ash {

// A fake version of `VideoConferenceTrayEffectsManager` that may be used for
// testing. The primary reason for this class's existence is to get around the
// condition that
// `VideoConferenceTrayEffectsManager::GetUiControllerForEffectId()` uniquely
// maps effect ids to tile UI controllers (this condition is good for
// production, but it makes testing difficult). For instance, many tests use
// multiple dummy effects that all use id `VcEffectId::kTestEffect`; each of
// these effects would require a unique `video_conference::VcTileUiController`
// (so that the tiles don't use the same icon and title, for instance), but the
// default behavior of `GetUiControllerForEffectId()` can only support returning
// a single tile UI controller for any given id. Once http://b/312496000 is
// addressed, this class may no longer be needed.
class ASH_EXPORT FakeVideoConferenceTrayEffectsManager
    : public VideoConferenceTrayEffectsManager {
 public:
  FakeVideoConferenceTrayEffectsManager();
  FakeVideoConferenceTrayEffectsManager(
      const FakeVideoConferenceTrayEffectsManager&) = delete;
  FakeVideoConferenceTrayEffectsManager& operator=(
      const FakeVideoConferenceTrayEffectsManager&) = delete;
  ~FakeVideoConferenceTrayEffectsManager() override;

  // Associates `effect_id` with a list of DLC ids, which may be different from
  // the list of DLC ids returned by
  // `VideoConferenceTrayEffectsManager::GetDlcIdsForEffectId()`.
  void SetDlcIdsForEffectId(VcEffectId effect_id,
                            std::vector<std::string> dlc_ids);

  // VideoConferenceTrayEffectsManager:
  video_conference::VcTileUiController* GetUiControllerForEffectId(
      VcEffectId effect_id) override;
  std::vector<std::string> GetDlcIdsForEffectId(VcEffectId effect_id) override;
  void UnregisterDelegate(VcEffectsDelegate* delegate) override;

 private:
  // An index into `GetToggleEffects()` and, simultaneously,
  // `tile_ui_controllers_`. This is incremented, modulo
  // `GetToggleEffects().size()`, every time `GetUiControllerForEffectId()` is
  // called, thus allowing the same effect id to potentially map to multiple
  // different UI controllers.
  size_t toggle_effects_index_ = 0;

  // A vector of (unique pointer to) tile UI controllers. A new tile UI
  // controller is created and added to this vector whenever
  // `GetUiControllerForEffectId()` is called and `toggle_effects_index_` is out
  // of this vector's range. Owned by `this`.
  std::vector<std::unique_ptr<video_conference::VcTileUiController>>
      tile_ui_controllers_;

  // A map from effect id to list of DLC id. Populate this via calls to
  // `SetDlcIdsForEffectId()`.
  std::map<VcEffectId, std::vector<std::string>> dlc_ids_for_effect_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_FAKE_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_H_
