// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DETACHABLE_BASE_DETACHABLE_BASE_HANDLER_H_
#define ASH_DETACHABLE_BASE_DETACHABLE_BASE_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chromeos/dbus/hammerd/hammerd_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

class DetachableBaseObserver;
struct UserInfo;

// Keeps track of the state of Chrome OS device's detachable base. It tracks
// whether:
//   * a detachable base is paired, and authenticated
//       * Detachable base is considered paired when hammerd connects Chrome OS
//         with the base
//       * During pairing, hammerd will issue a challenge to the detachable base
//         to verify its identity. The base is authenticated if the challenge
//         succeeds. If the pairing challenge fails, the base might still work,
//         but it will not be identifiable.
//   * the paired base requires a firmware update
// This information is used to detect when a detachable base related
// notification should be shown to the user.
// Note that this does not track active/existing users, but it provides methods
// to set and retrive per user state to and from local state - the
// DetachableBaseHandler clients are expected to determine for which users the
// detachable base state should be set or retrieved.
class ASH_EXPORT DetachableBaseHandler
    : public chromeos::HammerdClient::Observer,
      public chromeos::PowerManagerClient::Observer {
 public:
  // |local_state| - PrefService of Local state. May be null in tests.
  explicit DetachableBaseHandler(PrefService* local_state);
  ~DetachableBaseHandler() override;

  // Registers the local state prefs for detachable base devices.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void AddObserver(DetachableBaseObserver* observer);
  void RemoveObserver(DetachableBaseObserver* observer);

  // Removes the detachable base data associated with a user from local state.
  void RemoveUserData(const UserInfo& user);

  // Gets the detachable base pairing state.
  DetachableBasePairingStatus GetPairingStatus() const;

  // Whether the paired base matches the last base used by the user.
  // Returns true only if the base has kAuthenticated pairing state.
  // If this the user has not previously used any detachable bases (i.e. the
  // last used detachable base is empty), this will return true.
  bool PairedBaseMatchesLastUsedByUser(const UserInfo& user) const;

  // Sets the currently paired base as the last used base for the user.
  // If the user is not ephemeral, the last used base information will be
  // persisted in the local state.
  // No-op if a detachable base is not currently paired and authenticated.
  // Returns whether the base state was successfully updated.
  //
  // Note that this will fail for non-ephemeral users if |local_state_| is not
  // yet initialized. Observers will be notified that pairing status
  // changed when local state is initialized (provided a base is still
  // paired) - setting the last used base can be retried at that point.
  bool SetPairedBaseAsLastUsedByUser(const UserInfo& user);

  // chromeos::HammerdClient::Observer:
  void BaseFirmwareUpdateNeeded() override;
  void BaseFirmwareUpdateStarted() override;
  void BaseFirmwareUpdateSucceeded() override;
  void BaseFirmwareUpdateFailed() override;
  void PairChallengeSucceeded(const std::vector<uint8_t>& base_id) override;
  void PairChallengeFailed() override;
  void InvalidBaseConnected() override;

  // chromeos::PowerManagerClient::Observer:
  void TabletModeEventReceived(chromeos::PowerManagerClient::TabletMode mode,
                               const base::TimeTicks& timestamp) override;

 private:
  // Identifier for a detachable base device - HEX encoded string created from
  // data passed to PairChallengeSucceeded. It's known only if the base was
  // successfully authenticated.
  using DetachableBaseId = std::string;

  // Callback for getting initial power manager switches - used to determine
  // whether the tablet mode is on when the DetachableBaseHandler is created.
  void OnGotPowerManagerSwitchStates(
      base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // Updates the tracked tablet mode state, and notifies observers about pairing
  // status change if required.
  void UpdateTabletMode(chromeos::PowerManagerClient::TabletMode mode);

  // Retrieves the last known base used by a user. Returns an empty string if
  // a detachable base usage was not previously recorded for the user.
  DetachableBaseId GetLastUsedDeviceForUser(const UserInfo& user) const;

  // Notifies observers that the detachable base pairing state has changed.
  void NotifyPairingStatusChanged();

  // Notifies observers about whether the detachable base requires a firmware
  // update.
  void NotifyBaseRequiresFirmwareUpdate(bool requires_update);

  PrefService* local_state_ = nullptr;

  // Tablet mode state currently reported by power manager - tablet mode getting
  // turned on is used as a signal that the base is detached.
  base::Optional<chromeos::PowerManagerClient::TabletMode> tablet_mode_;

  // The HEX encoded ID of the authenticated paired base device. This will
  // be non empty iff pairing_status_ is kAuthenticated.
  DetachableBaseId authenticated_base_id_;

  // The paired base device pairing status.
  DetachableBasePairingStatus pairing_status_ =
      DetachableBasePairingStatus::kNone;

  ScopedObserver<chromeos::HammerdClient, chromeos::HammerdClient::Observer>
      hammerd_observer_;
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_observer_;

  // In-memory map from a user account ID to last used device set for user using
  // SetPairedBaseAsLastUsedByUser().
  // Used for ephemeral users.
  std::map<AccountId, DetachableBaseId> last_used_devices_;

  base::ObserverList<DetachableBaseObserver>::Unchecked observers_;

  base::WeakPtrFactory<DetachableBaseHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DetachableBaseHandler);
};

}  // namespace ash

#endif  // ASH_DETACHABLE_BASE_DETACHABLE_BASE_HANDLER_H_
