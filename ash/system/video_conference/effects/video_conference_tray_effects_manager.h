// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

namespace video_conference {
class VcTileUiController;
}  // namespace video_conference

class VcEffectsDelegate;
enum class VcEffectId;
class VcHostedEffect;

// The interface used to construct the UI that exposes video
// conferencing camera/microphone effects to the user.
class ASH_EXPORT VideoConferenceTrayEffectsManager {
 public:
  VideoConferenceTrayEffectsManager();

  VideoConferenceTrayEffectsManager(const VideoConferenceTrayEffectsManager&) =
      delete;
  VideoConferenceTrayEffectsManager& operator=(
      const VideoConferenceTrayEffectsManager&) = delete;

  virtual ~VideoConferenceTrayEffectsManager();

  class Observer : public base::CheckedObserver {
   public:
    // Called when an affect has change its support state.
    virtual void OnEffectSupportStateChanged(VcEffectId effect_id,
                                             bool is_supported) {}

    // Called when an effect changes. Currently, only observes
    // `kStudioLook` effect.
    virtual void OnEffectChanged(VcEffectId effect_id, bool is_on) {}

    // Called when the video conference bubble is opened.
    virtual void OnVideoConferenceBubbleOpened() {}
  };

  // Adds/removes `VideoConferenceTrayEffectsManager::Observer`.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Register/unregister a delegate that hosts one or more effects. When VcDlcUi
  // is enabled, tile controllers will be added/removed for supported effects.
  // Note: `UnregisterDelegate()` should only be overridden for testing
  // purposes, and only by `FakeVideoConferenceTrayEffectsManager`.
  void RegisterDelegate(VcEffectsDelegate* delegate);
  virtual void UnregisterDelegate(VcEffectsDelegate* delegate);

  // Returns 'true' if `delegate` is registered, 'false' otherwise.
  bool IsDelegateRegistered(VcEffectsDelegate* delegate);

  // A vector (or row) of `VcHostedEffect` objects of type
  // `VcEffectType::kToggle`.
  using EffectDataVector = std::vector<const VcHostedEffect*>;

  // A table of `VcHostedEffect` objects, intended to
  // represent the arrangement of toggle effect buttons in the bubble.
  using EffectDataTable = std::vector<EffectDataVector>;

  // Returns 'true' if there are any `VcHostedEffect`
  // objects of type `VcEffectType::kToggle`, 'false'
  // otherwise.
  bool HasToggleEffects();

  // Returns a pre-arranged table of `VcEffectsDelegate`
  // objects.
  EffectDataTable GetToggleEffectButtonTable();

  // Returns 'true' if there are any `VcHostedEffect`
  // objects of type `VcEffectType::kSetValue`, 'false'
  // otherwise.
  bool HasSetValueEffects();

  // Returns a vector of `VcHostedEffect` objects of type
  // `VcEffectType::kSetValue`, in no special order.
  EffectDataVector GetSetValueEffects();

  // Returns a vector of `VcHostedEffect` objects of type
  // `VcEffectType::kToggle`, in no special order.
  EffectDataVector GetToggleEffects();

  // Notifies all observers about effect support state changed.
  void NotifyEffectSupportStateChanged(VcEffectId effect_id, bool is_supported);

  // Notifies all observers about effect state changed.
  void NotifyEffectChanged(VcEffectId effect_id, bool is_on);

  // Notifies all observers about the video conference bubble opened.
  void NotifyVideoConferenceBubbleOpened();

  // Records the current state of all effects to metrics.
  void RecordInitialStates();

  // Returns a pointer to the UI controller of the tile associated with the
  // given `effect_id`. May return nullptr, for instance if there is no
  // associated UI controller for the given `effect_id`.
  // Should only be called when `VcDlcUi` is enabled.
  // Note: This should only be overridden for testing purposes, and only by
  // `FakeVideoConferenceTrayEffectsManager`.
  virtual video_conference::VcTileUiController* GetUiControllerForEffectId(
      VcEffectId effect_id);

  // Returns the DLC ids associated with `effect_id`. A VC effect may be
  // associated with zero, one, or multiple DLCs; the length of the returned
  // vector corresponds to how many DLCs are associated with the effect.
  // Note: This should only be overridden for testing purposes, and only by
  // `FakeVideoConferenceTrayEffectsManager`.
  virtual std::vector<std::string> GetDlcIdsForEffectId(VcEffectId effect_id);

 private:
  // Returns a vector of `VcHostedEffect` objects of type
  // `VcEffectType::kToggle`, in no special order.
  EffectDataVector GetTotalToggleEffectButtons();

  // Removes tile controllers for each toggle effect hosted by `delegate`. Note:
  // until http://b/298692153 is completed not every toggle effect will
  // necessarily have an associated tile controller.
  void RemoveTileControllers(VcEffectsDelegate* delegate);

  // A map from `VcEffectId` to (unique pointer to)
  // `video_conference::VcTileUiController`. This (potentially) gets updated
  // whenever a client requests a tile controller or whenever a
  // `VcEffectsDelegate` is unregistered.
  base::flat_map<VcEffectId,
                 std::unique_ptr<video_conference::VcTileUiController>>
      controller_for_effect_id_;

  // This list of registered effect delegates, unowned.
  std::vector<raw_ptr<VcEffectsDelegate, VectorExperimental>> effect_delegates_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<VideoConferenceTrayEffectsManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_H_
