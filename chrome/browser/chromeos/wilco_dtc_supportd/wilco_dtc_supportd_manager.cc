// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_bridge.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

WilcoDtcSupportdManager* g_wilco_dtc_supportd_manager_instance = nullptr;

class WilcoDtcSupportdManagerDelegateImpl final
    : public WilcoDtcSupportdManager::Delegate {
 public:
  WilcoDtcSupportdManagerDelegateImpl();
  ~WilcoDtcSupportdManagerDelegateImpl() override;

  // Delegate overrides:
  std::unique_ptr<WilcoDtcSupportdBridge> CreateWilcoDtcSupportdBridge()
      override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdManagerDelegateImpl);
};

WilcoDtcSupportdManagerDelegateImpl::WilcoDtcSupportdManagerDelegateImpl() =
    default;

WilcoDtcSupportdManagerDelegateImpl::~WilcoDtcSupportdManagerDelegateImpl() =
    default;

std::unique_ptr<WilcoDtcSupportdBridge>
WilcoDtcSupportdManagerDelegateImpl::CreateWilcoDtcSupportdBridge() {
  return std::make_unique<WilcoDtcSupportdBridge>(
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory());
}

// Returns true if only affiliated users are logged-in.
bool AreOnlyAffiliatedUsersLoggedIn() {
  const user_manager::UserList logged_in_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::User* user : logged_in_users) {
    if (!user->IsAffiliated()) {
      return false;
    }
  }
  return true;
}

}  // namespace

WilcoDtcSupportdManager::Delegate::~Delegate() = default;

// static
WilcoDtcSupportdManager* WilcoDtcSupportdManager::Get() {
  return g_wilco_dtc_supportd_manager_instance;
}

WilcoDtcSupportdManager::WilcoDtcSupportdManager()
    : WilcoDtcSupportdManager(
          std::make_unique<WilcoDtcSupportdManagerDelegateImpl>()) {}

WilcoDtcSupportdManager::WilcoDtcSupportdManager(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  DCHECK(!g_wilco_dtc_supportd_manager_instance);
  g_wilco_dtc_supportd_manager_instance = this;
  wilco_dtc_allowed_observer_ = CrosSettings::Get()->AddSettingsObserver(
      kDeviceWilcoDtcAllowed,
      base::BindRepeating(&WilcoDtcSupportdManager::StartOrStopWilcoDtc,
                          weak_ptr_factory_.GetWeakPtr()));

  session_manager::SessionManager::Get()->AddObserver(this);

  StartOrStopWilcoDtc();
}

WilcoDtcSupportdManager::~WilcoDtcSupportdManager() {
  DCHECK_EQ(g_wilco_dtc_supportd_manager_instance, this);
  g_wilco_dtc_supportd_manager_instance = nullptr;

  // The destruction may mean that non-affiliated user is logging out.
  StartOrStopWilcoDtc();

  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void WilcoDtcSupportdManager::SetConfigurationData(
    std::unique_ptr<std::string> data) {
  configuration_data_ = std::move(data);

  if (!wilco_dtc_supportd_bridge_) {
    VLOG(0) << "Cannot send notification - no bridge to the daemon";
    return;
  }
  wilco_dtc_supportd_bridge_->SetConfigurationData(configuration_data_.get());

  wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceProxy* const
      wilco_dtc_supportd_mojo_proxy =
          wilco_dtc_supportd_bridge_->wilco_dtc_supportd_service_mojo_proxy();
  if (!wilco_dtc_supportd_mojo_proxy) {
    VLOG(0) << "Cannot send message - Mojo connection to the daemon isn't "
               "bootstrapped yet";
    return;
  }
  wilco_dtc_supportd_mojo_proxy->NotifyConfigurationDataChanged();
}

const std::string& WilcoDtcSupportdManager::GetConfigurationDataForTesting()
    const {
  return configuration_data_ ? *configuration_data_ : base::EmptyString();
}

void WilcoDtcSupportdManager::OnSessionStateChanged() {
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  // The user is logged-in and the affiliation is set.
  if (session_state == session_manager::SessionState::ACTIVE)
    StartOrStopWilcoDtc();
}

void WilcoDtcSupportdManager::StartOrStopWilcoDtc() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
  bool wilco_dtc_allowed;
  // Start wilco DTC support services only if logged-in users are affiliated
  // and the wilco DTC is allowed by policy.
  if (CrosSettings::Get()->GetBoolean(kDeviceWilcoDtcAllowed,
                                      &wilco_dtc_allowed) &&
      wilco_dtc_allowed && AreOnlyAffiliatedUsersLoggedIn()) {
    StartWilcoDtc(base::BindOnce(&WilcoDtcSupportdManager::OnStartWilcoDtc,
                                 callback_weak_ptr_factory_.GetWeakPtr()));
  } else {
    StopWilcoDtc(base::BindOnce(&WilcoDtcSupportdManager::OnStopWilcoDtc,
                                callback_weak_ptr_factory_.GetWeakPtr()));
  }
}

void WilcoDtcSupportdManager::StartWilcoDtc(WilcoDtcCallback callback) {
  VLOG(1) << "Starting wilco DTC";
  UpstartClient::Get()->StartWilcoDtcService(std::move(callback));
}

void WilcoDtcSupportdManager::StopWilcoDtc(WilcoDtcCallback callback) {
  VLOG(1) << "Stopping wilco DTC";
  UpstartClient::Get()->StopWilcoDtcService(std::move(callback));
}

void WilcoDtcSupportdManager::OnStartWilcoDtc(bool success) {
  if (!success)
    DLOG(ERROR) << "Failed to start the wilco DTC, it might be already running";
  else
    VLOG(1) << "Wilco DTC started";

  // The bridge has to be created regardless of a |success| value. When wilco
  // DTC is already running, it responds with an error on attempt to start it.
  if (!wilco_dtc_supportd_bridge_) {
    wilco_dtc_supportd_bridge_ = delegate_->CreateWilcoDtcSupportdBridge();
    DCHECK(wilco_dtc_supportd_bridge_);

    // Once the bridge is created, notify it about an available configuration
    // data blob.
    wilco_dtc_supportd_bridge_->SetConfigurationData(configuration_data_.get());
  }
}

void WilcoDtcSupportdManager::OnStopWilcoDtc(bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to stop wilco DTC";
  } else {
    VLOG(1) << "Wilco DTC stopped";
    wilco_dtc_supportd_bridge_.reset();
  }
}

}  // namespace chromeos
