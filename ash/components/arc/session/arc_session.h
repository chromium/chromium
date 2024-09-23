// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_SESSION_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_SESSION_H_

#include <memory>
#include <string>

#include "ash/components/arc/session/adb_sideloading_availability_delegate.h"
#include "ash/components/arc/session/arc_client_adapter.h"
#include "ash/components/arc/session/arc_stop_reason.h"
#include "ash/components/arc/session/arc_upgrade_params.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
class SchedulerConfigurationManagerBase;
}

namespace cryptohome {
class Identification;
}

namespace version_info {
enum class Channel;
}

namespace arc {

class ArcBridgeService;

// Starts the ARC instance and bootstraps the bridge connection.
// Clients should implement the Delegate to be notified upon communications
// being available.
// The instance can be safely removed before StartMiniInstance() is called, or
// after OnSessionStopped() is called.  The number of instances must be at
// most one. Otherwise, ARC instances will conflict.
class ArcSession {
 public:
  // Observer to notify events corresponding to one ARC session run.
  class Observer : public base::CheckedObserver {
   public:
    // Called when ARC instance is stopped. This is called exactly once per
    // instance.  |was_running| is true if the stopped instance was fully set
    // up and running. |full_requested| is true if the full container was
    // requested.
    virtual void OnSessionStopped(ArcStopReason reason,
                                  bool was_running,
                                  bool full_requested) = 0;

   protected:
    ~Observer() override = default;
  };

  // Creates a default instance of ArcSession.
  static std::unique_ptr<ArcSession> Create(
      ArcBridgeService* arc_bridge_service,
      version_info::Channel channel,
      ash::SchedulerConfigurationManagerBase* scheduler_configuration_manager,
      AdbSideloadingAvailabilityDelegate*
          adb_sideloading_availability_delegate);

  ArcSession(const ArcSession&) = delete;
  ArcSession& operator=(const ArcSession&) = delete;

  virtual ~ArcSession();

  // Sends D-Bus message to start a mini-container.
  virtual void StartMiniInstance() = 0;

  // Sends a D-Bus message to upgrade to a full instance if possible. This
  // might be done asynchronously; the message might only be sent after other
  // operations have completed.
  virtual void RequestUpgrade(UpgradeParams params) = 0;

  // Requests to stop the currently-running instance regardless of its mode.
  // The completion is notified via OnSessionStopped() of the Observer.
  virtual void Stop() = 0;

  // Returns true if Stop() has been called already.
  virtual bool IsStopRequested() = 0;

  // Called when Chrome is in shutdown state. This is called when the message
  // loop is already stopped, and the instance will soon be deleted. Caller
  // may expect that OnSessionStopped() is synchronously called back except
  // when it has already been called before.
  virtual void OnShutdown() = 0;

  // Sets a hash string of the profile user IDs and an ARC serial number for the
  // user.
  virtual void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                           const std::string& hash,
                           const std::string& serial_number) = 0;

  // Provides the DemoModeDelegate which will be used to load the demo session
  // apps path.
  virtual void SetDemoModeDelegate(
      ArcClientAdapter::DemoModeDelegate* delegate) = 0;

  // Trims VM's memory by moving it to zram.
  // When the operation is done |callback| is called.
  // If nonzero, |page_limit| defines the max number of pages to reclaim.
  static constexpr int kNoPageLimit = 0;
  using TrimVmMemoryCallback =
      base::OnceCallback<void(bool success, const std::string& failure_reason)>;
  virtual void TrimVmMemory(TrimVmMemoryCallback callback, int page_limit) = 0;

  // Sets the default display resolution scale factor.
  virtual void SetDefaultDeviceScaleFactor(float scale_factor) = 0;

  // Sets whether the device should use virtio-blk for /data user directory.
  virtual void SetUseVirtioBlkData(bool use_virtio_blk_data) = 0;

  // Sets whether the user is signed into ARC or provisioned.
  virtual void SetArcSignedIn(bool arc_signed_in) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ArcSession();

  base::ObserverList<Observer> observer_list_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_SESSION_H_
