// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_CLIENT_ADAPTER_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_CLIENT_ADAPTER_H_

#include <memory>
#include <string>

#include "ash/components/arc/session/arc_start_params.h"
#include "ash/components/arc/session/arc_upgrade_params.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/arc/arc.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace cryptohome {
class Identification;
}  // namespace cryptohome

namespace arc {

// An adapter to talk to a Chrome OS daemon to manage lifetime of ARC instance.
class ArcClientAdapter {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void ArcInstanceStopped(bool is_system_shutdown) = 0;
  };

  // DemoModeDelegate contains functions used to load the demo session apps for
  // ARC. The adapter cannot do this directly because ash::DemoSession classes
  // are in //chrome.
  class DemoModeDelegate {
   public:
    virtual ~DemoModeDelegate() = default;

    // Ensures that the demo session resources are loaded, if demo mode
    // is enabled. This must be called before GetDemoAppsPath().
    virtual void EnsureResourcesLoaded(base::OnceClosure callback) = 0;

    // Gets the path of the image containing demo session Android apps. Returns
    // an empty path if demo mode is not enabled.
    virtual base::FilePath GetDemoAppsPath() = 0;
  };

  // Creates a default instance of ArcClientAdapter.
  static std::unique_ptr<ArcClientAdapter> Create();

  // Convert StartParams to StartArcMiniInstanceRequest
  static StartArcMiniInstanceRequest
  ConvertStartParamsToStartArcMiniInstanceRequest(const StartParams& params);

  ArcClientAdapter(const ArcClientAdapter&) = delete;
  ArcClientAdapter& operator=(const ArcClientAdapter&) = delete;

  virtual ~ArcClientAdapter();

  // StartMiniArc starts ARC with only a handful of ARC processes for Chrome OS
  // login screen.
  virtual void StartMiniArc(StartParams params,
                            chromeos::VoidDBusMethodCallback callback) = 0;

  // UpgradeArc upgrades a mini ARC instance to a full ARC instance.
  virtual void UpgradeArc(UpgradeParams params,
                          chromeos::VoidDBusMethodCallback callback) = 0;

  // Asynchronously stops the ARC instance. |on_shutdown| is true if the method
  // is called due to the browser being shut down. Also backs up the ARC
  // bug report if |should_backup_log| is set to true.
  virtual void StopArcInstance(bool on_shutdown, bool should_backup_log) = 0;

  // Sets a hash string of the profile user IDs and an ARC serial number for the
  // user.
  virtual void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                           const std::string& hash,
                           const std::string& serial_number) = 0;

  // Provides the DemoModeDelegate which will be used to load the demo session
  // apps path.
  virtual void SetDemoModeDelegate(DemoModeDelegate* delegate) = 0;

  // Trims VM's memory by moving it to zram.
  // When the operation is done |callback| is called.
  // If nonzero, |page_limit| defines the max number of pages to reclaim.
  using TrimVmMemoryCallback =
      base::OnceCallback<void(bool success, const std::string& failure_reason)>;
  virtual void TrimVmMemory(TrimVmMemoryCallback callback, int page_limit) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ArcClientAdapter();

  base::ObserverList<Observer> observer_list_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_CLIENT_ADAPTER_H_
