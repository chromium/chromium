// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_wifi_host_impl.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/technology_state_controller.h"

namespace {

ash::NetworkStateHandler* GetStateHandler() {
  return ash::NetworkHandler::Get()->network_state_handler();
}

ash::TechnologyStateController* GetTechnologyStateController() {
  return ash::NetworkHandler::Get()->technology_state_controller();
}

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcWifiHostImpl.
class ArcWifiHostImplFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcWifiHostImpl,
          ArcWifiHostImplFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcWifiHostImplFactory";

  static ArcWifiHostImplFactory* GetInstance() {
    return base::Singleton<ArcWifiHostImplFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcWifiHostImplFactory>;
  ArcWifiHostImplFactory() = default;
  ~ArcWifiHostImplFactory() override = default;
};

}  // namespace

// static
ArcWifiHostImpl* ArcWifiHostImpl::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcWifiHostImplFactory::GetForBrowserContext(context);
}

// static
ArcWifiHostImpl* ArcWifiHostImpl::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcWifiHostImplFactory::GetForBrowserContextForTesting(context);
}

ArcWifiHostImpl::ArcWifiHostImpl(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->arc_wifi()->SetHost(this);
  arc_bridge_service_->arc_wifi()->AddObserver(this);
}

ArcWifiHostImpl::~ArcWifiHostImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->arc_wifi()->RemoveObserver(this);
  arc_bridge_service_->arc_wifi()->SetHost(nullptr);
}

// static
void ArcWifiHostImpl::EnsureFactoryBuilt() {
  ArcWifiHostImplFactory::GetInstance();
}

void ArcWifiHostImpl::GetWifiEnabledState(
    GetWifiEnabledStateCallback callback) {
  bool is_enabled =
      GetStateHandler()->IsTechnologyEnabled(ash::NetworkTypePattern::WiFi());
  std::move(callback).Run(is_enabled);
}

void ArcWifiHostImpl::SetWifiEnabledState(
    bool is_enabled,
    SetWifiEnabledStateCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto state =
      GetStateHandler()->GetTechnologyState(ash::NetworkTypePattern::WiFi());
  // WiFi can't be enabled or disabled in these states.
  switch (state) {
    case ash::NetworkStateHandler::TECHNOLOGY_PROHIBITED:
    case ash::NetworkStateHandler::TECHNOLOGY_UNINITIALIZED:
    case ash::NetworkStateHandler::TECHNOLOGY_UNAVAILABLE:
      // If WiFi is in above state, it is already disabled. This is a noop.
      if (!is_enabled) {
        std::move(callback).Run(true);
        return;
      }
      NET_LOG(ERROR) << __func__ << ": failed due to WiFi state: " << state;
      std::move(callback).Run(false);
      return;
    default:
      break;
  }

  NET_LOG(USER) << __func__ << ": " << is_enabled;
  GetTechnologyStateController()->SetTechnologiesEnabled(
      ash::NetworkTypePattern::WiFi(), is_enabled,
      ash::network_handler::ErrorCallback());
  std::move(callback).Run(true);
}

}  // namespace arc
