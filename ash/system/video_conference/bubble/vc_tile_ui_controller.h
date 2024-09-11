// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_VC_TILE_UI_CONTROLLER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_VC_TILE_UI_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace ash {

class VcEffectState;
enum class VcEffectId;

namespace video_conference {

// The controller for the UI of toggle tiles in the VC controls bubble. There is
// one controller per tile. Non-UI logic for the tiles is handled by
// `ash::VcEffectsDelegate`.
//
// Note: This class is only used when `VcDlcUi` is enabled.
class ASH_EXPORT VcTileUiController
    : public DlcserviceClient::Observer,
      public VideoConferenceTrayEffectsManager::Observer {
 public:
  explicit VcTileUiController(const VcHostedEffect* effect);
  VcTileUiController(const VcTileUiController&) = delete;
  VcTileUiController& operator=(const VcTileUiController&) = delete;
  ~VcTileUiController() override;

  // Creates and returns the `FeatureTile` associated with this controller.
  std::unique_ptr<FeatureTile> CreateTile();

  // Returns the effect id associated with this controller, for testing purposes
  // only.
  VcEffectId effect_id_for_testing() const { return effect_id_; }

 private:
  friend class VcTileUiControllerTest;

  // Starts a request for DLC download state. Multiple DLCs may be available.
  class DlcDownloadStateRequest {
   public:
    DlcDownloadStateRequest(
        const base::flat_set<std::string>& dlc_ids,
        base::OnceCallback<void(FeatureTile::DownloadState download_state,
                                int progress)> set_progress_callback);
    DlcDownloadStateRequest(const DlcDownloadStateRequest&) = delete;
    DlcDownloadStateRequest& operator=(const DlcDownloadStateRequest&) = delete;
    ~DlcDownloadStateRequest();

   private:
    // A collection of information related to the download state of a particular
    // DLC.
    struct DlcDownloadState {
      const std::string id;
      const std::string error_code;
      const dlcservice::DlcState dlc_state;
    };

    // Callback passed to `DlcServiceClient` to record DlcState. May be called
    // multiple times if multiple `dlc_id`'s need to be checked.
    void OnDlcStateRetrieved(
        std::string dlc_id,
        base::OnceCallback<void(DlcDownloadState)> merge_callback,
        std::string_view error,
        const dlcservice::DlcState& dlc_state);

    // Callback called when all dlc states have been retrieved.
    void OnAllDlcStatesRetrieved(
        std::vector<DlcDownloadState> dlc_download_states);

    // Callback called after all DLC states are received. Results in destruction
    // of the `DlcDownloadStateRequest`.
    base::OnceCallback<void(FeatureTile::DownloadState download_state,
                            int progress)>
        set_progress_callback_;

    base::WeakPtrFactory<DlcDownloadStateRequest> weak_ptr_factory_{this};
  };

  // DlcserviceClient::Observer:
  void OnDlcStateChanged(const dlcservice::DlcState& dlc_state) override;

  // VideoConferenceTrayEffectsManager::Observer:
  void OnEffectChanged(VcEffectId effect_id, bool is_on) override;

  // Called when the `FeatureTile` associated with this controller is pressed.
  void OnPressed(const ui::Event& event);

  // Sets the tooltip text based on the associated tile's toggle state.
  void UpdateTooltip();

  // Tracks the toggling behavior. Obtains the histogram name with the help of
  // `GetEffectId()`.
  void TrackToggleUMA(bool target_toggle_state);

  // Plays a haptic effect based on `target_toggle_state`.
  void PlayToggleHaptic(bool target_toggle_state);

  // Updates the UI of the tile managed by this controller based on the download
  // progress of the tile's associated DLC.
  void UpdateDlcDownloadUi();

  // Callback passed to `dlc_download_state_request_`, called when all download
  // states related to `dlc_ids_` have been fetched.
  void OnDlcDownloadStateFetched(FeatureTile::DownloadState download_state,
                                 int progress);

  // A weak pointer to the `FeatureTile` associated with this controller. Note
  // that this is guaranteed to be nullptr before `CreateTile()` is called, and
  // may even be nullptr after `CreateTile()` is called (the tile is owned by
  // the Views hierarchy, not this controller).
  base::WeakPtr<FeatureTile> tile_ = nullptr;

  // The effect id which is used for UMA tracking.
  VcEffectId effect_id_;

  // Information about the associated video conferencing effect needed to
  // display the UI of the tile controlled by this controller. WeakPtr's are
  // saved because the `VcTileUiController` may outlive its dependencies.
  base::WeakPtr<const VcEffectState> effect_state_;
  base::WeakPtr<const VcHostedEffect> effect_;

  // The initial label for `effect_state_`, used for debugging.
  std::u16string effect_state_label_for_debug_;

  // A list of ids for the DLCs associated with the tile managed by this
  // controller. This is empty for tiles not associated with any DLC.
  base::flat_set<std::string> dlc_ids_;

  // A request which asynchronously fetches 0->many DLC download states, and
  // updates the controller on combined progress or any errors.
  std::unique_ptr<DlcDownloadStateRequest> dlc_download_state_request_;

  base::WeakPtrFactory<VcTileUiController> weak_ptr_factory_{this};
};

}  // namespace video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_VC_TILE_UI_CONTROLLER_H_
