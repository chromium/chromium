// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_POWER_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_POWER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_manager/user_type.h"
#include "device/bluetooth/bluetooth_adapter.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// Listens to changes of bluetooth power preferences and apply them to the
// device. Also initializes the bluetooth power during system startup
// and user session startup.
//
// This should be the only entity controlling bluetooth power. All other code
// outside ash that wants to control bluetooth power should set user pref
// setting instead.
class ASH_EXPORT BluetoothPowerController
    : public SessionObserver,
      public device::BluetoothAdapter::Observer {
 public:
  explicit BluetoothPowerController(PrefService* local_state);
  ~BluetoothPowerController() override;

  // Changes the bluetooth power setting to |enabled|.
  void SetBluetoothEnabled(bool enabled);

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets the primary user's bluetooth power pref. Setting the pref will also
  // trigger the change of the bluetooth power. This method can only be called
  // when the primary user is the active user, otherwise the operation is
  // ignored.
  void SetPrimaryUserBluetoothPowerSetting(bool enabled);

  // Called when BluetoothAdapterFactory::GetAdapter is ready to pass the
  // adapter pointer.
  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;

  device::BluetoothAdapter* bluetooth_adapter_for_test() {
    return bluetooth_adapter_.get();
  }

 private:
  friend class BluetoothPowerControllerTest;

  void StartWatchingActiveUserPrefsChanges();
  void StartWatchingLocalStatePrefsChanges();
  void StopWatchingActiveUserPrefsChanges();

  void OnBluetoothPowerActiveUserPrefChanged();
  void OnBluetoothPowerLocalStatePrefChanged();

  // At primary user session startup, apply the user's bluetooth power setting
  // or set the default if the user doesn't have the setting yet.
  void ApplyBluetoothPrimaryUserPref();

  // At login screen startup, apply the local state bluetooth power setting
  // or set the default if the device doesn't have the setting yet.
  void ApplyBluetoothLocalStatePref();

  // Sets the bluetooth power, may defer the operation if bluetooth adapter
  // is not yet ready.
  void SetBluetoothPower(bool enabled);

  // Sets the bluetooth power given the ready adapter.
  void SetBluetoothPowerOnAdapterReady();

  using BluetoothTask = base::OnceClosure;

  // If adapter is ready run the task right now, otherwise add the task
  // to the queue which will be executed when bluetooth adapter is ready.
  void RunBluetoothTaskWhenAdapterReady(BluetoothTask task);

  // Triggers running all pending bluetooth adapter-dependent tasks.
  void TriggerRunPendingBluetoothTasks();

  // Runs the next pending bluetooth task. This will trigger another
  // RunNextPendingBluetoothTask when the current task is finished. It will stop
  // executing the next task when the adapter becomes not present or the queue
  // has been empty.
  void RunNextPendingBluetoothTask();

  // Sets the pref value based on current bluetooth power state. May defer
  // the operation if bluetooth adapter is not ready yet.
  void SavePrefValue(PrefService* prefs, const char* pref_name);

  // Sets the pref value based on current bluetooth power state, given the ready
  // bluetooth adapter.
  void SavePrefValueOnAdapterReady(PrefService* prefs, const char* pref_name);

  // Decides whether to apply bluetooth setting based on user type.
  // Returns true if the user type represents a human individual, currently this
  // includes: regular, child, supervised, or active directory. The other types
  // do not represent human account so those account should follow system-wide
  // bluetooth setting instead.
  bool ShouldApplyUserBluetoothSetting(user_manager::UserType user_type) const;

  // Remembers whether we have ever applied the primary user's bluetooth
  // setting. If this variable is true, we will ignore any active user change
  // event since we know that the primary user's bluetooth setting has been
  // applied.
  bool is_primary_user_bluetooth_applied_ = false;

  PrefService* active_user_pref_service_ = nullptr;
  PrefService* local_state_ = nullptr;

  // Contains pending tasks which depend on the availability of bluetooth
  // adapter.
  base::queue<BluetoothTask> pending_bluetooth_tasks_;

  // The registrar used to watch prefs changes in the above
  // |active_user_pref_service_| from outside ash.
  // NOTE: Prefs are how Chrome communicates changes to the bluetooth power
  // settings controlled by this class from the WebUI settings.
  std::unique_ptr<PrefChangeRegistrar> active_user_pref_change_registrar_;
  std::unique_ptr<PrefChangeRegistrar> local_state_pref_change_registrar_;

  // True indicates that pending_bluetooth_tasks_ is being executed and
  // waiting for complete callback.
  bool pending_tasks_busy_ = false;

  // If not empty this indicates the pending target bluetooth power to be set.
  // This needs to be tracked so that we can combine multiple pending power
  // change requests.
  base::Optional<bool> pending_bluetooth_power_target_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  base::WeakPtrFactory<BluetoothPowerController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothPowerController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_POWER_CONTROLLER_H_
