// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_container_client_adapter.h"

#include <string>
#include <utility>

#include "ash/components/arc/session/arc_session.h"
#include "ash/components/arc/session/arc_upgrade_params.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/arc/arc.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace arc {
namespace {

// Converts PackageCacheMode into login_manager's.
UpgradeArcContainerRequest_PackageCacheMode ToLoginManagerPackageCacheMode(
    UpgradeParams::PackageCacheMode mode) {
  switch (mode) {
    case UpgradeParams::PackageCacheMode::DEFAULT:
      return UpgradeArcContainerRequest_PackageCacheMode_DEFAULT;
    case UpgradeParams::PackageCacheMode::COPY_ON_INIT:
      return UpgradeArcContainerRequest_PackageCacheMode_COPY_ON_INIT;
    case UpgradeParams::PackageCacheMode::SKIP_SETUP_COPY_ON_INIT:
      return UpgradeArcContainerRequest_PackageCacheMode_SKIP_SETUP_COPY_ON_INIT;
  }
}

// Converts ArcManagementTransition into login_manager's.
UpgradeArcContainerRequest_ManagementTransition
ToLoginManagerManagementTransition(ArcManagementTransition transition) {
  switch (transition) {
    case ArcManagementTransition::NO_TRANSITION:
      return UpgradeArcContainerRequest_ManagementTransition_NONE;
    case ArcManagementTransition::CHILD_TO_REGULAR:
      return UpgradeArcContainerRequest_ManagementTransition_CHILD_TO_REGULAR;
    case ArcManagementTransition::REGULAR_TO_CHILD:
      return UpgradeArcContainerRequest_ManagementTransition_REGULAR_TO_CHILD;
    case ArcManagementTransition::UNMANAGED_TO_MANAGED:
      return UpgradeArcContainerRequest_ManagementTransition_UNMANAGED_TO_MANAGED;
  }
}

}  // namespace

class ArcContainerClientAdapter : public ArcClientAdapter,
                                  public ash::SessionManagerClient::Observer {
 public:
  ArcContainerClientAdapter() {
    if (ash::SessionManagerClient::Get())
      ash::SessionManagerClient::Get()->AddObserver(this);
  }

  ArcContainerClientAdapter(const ArcContainerClientAdapter&) = delete;
  ArcContainerClientAdapter& operator=(const ArcContainerClientAdapter&) =
      delete;

  ~ArcContainerClientAdapter() override {
    if (ash::SessionManagerClient::Get())
      ash::SessionManagerClient::Get()->RemoveObserver(this);
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(StartParams params,
                    chromeos::VoidDBusMethodCallback callback) override {
    auto request =
        ArcClientAdapter::ConvertStartParamsToStartArcMiniInstanceRequest(
            std::move(params));
    ash::SessionManagerClient::Get()->StartArcMiniContainer(
        request, std::move(callback));
  }

  UpgradeArcContainerRequest ConvertUpgradeParamsToUpgradeArcContainerRequest(
      UpgradeParams params) {
    UpgradeArcContainerRequest request;
    request.set_account_id(params.account_id);
    request.set_is_account_managed(params.is_account_managed);
    request.set_is_managed_adb_sideloading_allowed(
        params.is_managed_adb_sideloading_allowed);
    request.set_skip_boot_completed_broadcast(
        params.skip_boot_completed_broadcast);
    request.set_packages_cache_mode(
        ToLoginManagerPackageCacheMode(params.packages_cache_mode));
    request.set_skip_gms_core_cache(params.skip_gms_core_cache);
    request.set_skip_tts_cache(params.skip_tts_cache);
    request.set_is_demo_session(params.is_demo_session);
    request.set_demo_session_apps_path(params.demo_session_apps_path.value());
    request.set_locale(params.locale);
    request.set_enable_arc_nearby_share(params.enable_arc_nearby_share);
    for (const auto& language : params.preferred_languages)
      request.add_preferred_languages(language);
    request.set_management_transition(
        ToLoginManagerManagementTransition(params.management_transition));
    return request;
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    auto request = ConvertUpgradeParamsToUpgradeArcContainerRequest(params);
    ash::SessionManagerClient::Get()->UpgradeArcContainer(request,
                                                          std::move(callback));
  }

  void StopArcInstance(bool on_shutdown, bool should_backup_log) override {
    // Since we have the ArcInstanceStopped() callback, we don't need to do
    // anything when StopArcInstance completes.
    ash::SessionManagerClient::Get()->StopArcInstance(
        cryptohome_id_.id(), should_backup_log, base::DoNothing());
  }

  void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                   const std::string& hash,
                   const std::string& serial_number) override {
    DCHECK(cryptohome_id_.id().empty());
    if (cryptohome_id.id().empty())
      LOG(WARNING) << "cryptohome_id is empty";
    cryptohome_id_ = cryptohome_id;
  }

  // ArcContainerClientAdapter gets the demo session apps path from
  // UpgradeParams, so it does not use the DemoModeDelegate.
  void SetDemoModeDelegate(DemoModeDelegate* delegate) override {}

  // The interface is only for ARCVM.
  void TrimVmMemory(TrimVmMemoryCallback callback, int) override {
    NOTREACHED();
  }

  // ash::SessionManagerClient::Observer overrides:
  void ArcInstanceStopped(
      login_manager::ArcContainerStopReason reason) override {
    const bool is_system_shutdown =
        reason ==
        login_manager::ArcContainerStopReason::SESSION_MANAGER_SHUTDOWN;
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped(is_system_shutdown);
  }

 private:
  // A cryptohome ID of the primary profile.
  cryptohome::Identification cryptohome_id_;
};

std::unique_ptr<ArcClientAdapter> CreateArcContainerClientAdapter() {
  return std::make_unique<ArcContainerClientAdapter>();
}

}  // namespace arc
