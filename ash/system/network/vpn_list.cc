// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_list.h"

#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/tray_network_state_model.h"
#include "base/functional/bind.h"
#include "base/notreached.h"

namespace ash {

VpnList::Observer::~Observer() = default;

VpnList::VpnList(TrayNetworkStateModel* model) : model_(model) {
  model_->AddObserver(this);
  AddBuiltInProvider();
  VpnProvidersChanged();
}

VpnList::~VpnList() {
  model_->RemoveObserver(this);
}

bool VpnList::HaveExtensionOrArcVpnProviders() const {
  for (const VpnProviderPtr& extension_provider : extension_vpn_providers_) {
    if (extension_provider->type == VpnType::kExtension)
      return true;
  }
  return arc_vpn_providers_.size() > 0;
}

void VpnList::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VpnList::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void VpnList::ActiveNetworkStateChanged() {
  Update();
}

void VpnList::VpnProvidersChanged() {
  Update();
}

void VpnList::SetVpnProvidersForTest(std::vector<VpnProviderPtr> providers) {
  OnGetVpnProviders(std::move(providers));
}

void VpnList::OnGetVpnProviders(std::vector<VpnProviderPtr> providers) {
  extension_vpn_providers_.clear();
  arc_vpn_providers_.clear();
  AddBuiltInProvider();
  // Add Third Party (Extension and Arc) providers.
  for (auto& provider : providers) {
    switch (provider->type) {
      case VpnType::kIKEv2:
      case VpnType::kL2TPIPsec:
      case VpnType::kOpenVPN:
      case VpnType::kWireGuard:
        // Only third party VpnProvider instances should exist.
        NOTREACHED();
      case VpnType::kExtension:
        extension_vpn_providers_.push_back(std::move(provider));
        break;
      case VpnType::kArc:
        arc_vpn_providers_.push_back(std::move(provider));
        break;
    }
  }
  NotifyObservers();
}

void VpnList::NotifyObservers() {
  for (auto& observer : observer_list_)
    observer.OnVpnProvidersChanged();
}

void VpnList::AddBuiltInProvider() {
  // Create a VpnProvider with type kOpenVPN for the built-in provider.
  extension_vpn_providers_.push_back(
      VpnProvider::New(VpnType::kOpenVPN,
                       /*provider_id=*/std::string(),
                       /*provider_name=*/std::string(),
                       /*app_id=*/std::string(),
                       /*last_launch_time=*/base::Time()));
}

void VpnList::Update() {
  model_->cros_network_config()->GetVpnProviders(
      base::BindOnce(&VpnList::OnGetVpnProviders, base::Unretained(this)));
}

}  // namespace ash
