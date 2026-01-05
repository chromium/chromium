// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/vpn_service_ash.h"

#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_driver_client.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "crypto/sha2.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace crosapi {

VpnServiceForExtensionAsh::VpnServiceForExtensionAsh(
    const std::string& extension_id,
    chromeos::VpnService* controller)
    : extension_id_(extension_id), controller_(controller) {
}

VpnServiceForExtensionAsh::~VpnServiceForExtensionAsh() = default;

void VpnServiceForExtensionAsh::BindReceiverAndObserver(
    mojo::PendingReceiver<crosapi::mojom::VpnServiceForExtension> receiver,
    mojo::PendingRemote<crosapi::mojom::EventObserverForExtension> observer) {
  receivers_.Add(this, std::move(receiver));
  observers_.Add(std::move(observer));
}

void VpnServiceForExtensionAsh::DispatchConfigRemovedEvent(
    const std::string& configuration_name) {
  for (auto& observer : observers_) {
    observer->OnConfigRemoved(configuration_name);
  }
}

void VpnServiceForExtensionAsh::DispatchOnPacketReceivedEvent(
    const std::vector<char>& data) {
  std::vector<uint8_t> data_(data.begin(), data.end());
  for (auto& observer : observers_) {
    observer->OnPacketReceived(data_);
  }
}

void VpnServiceForExtensionAsh::DispatchOnPlatformMessageEvent(
    const std::string& configuration_name,
    int32_t platform_message) {
  for (auto& observer : observers_) {
    observer->OnPlatformMessage(configuration_name, platform_message);
  }
}

VpnServiceAsh::VpnServiceAsh() = default;

VpnServiceAsh::~VpnServiceAsh() = default;

void VpnServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::VpnService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void VpnServiceAsh::RegisterVpnServiceForExtension(
    const std::string& extension_id,
    mojo::PendingReceiver<crosapi::mojom::VpnServiceForExtension> receiver,
    mojo::PendingRemote<crosapi::mojom::EventObserverForExtension> observer) {
  auto* service = GetVpnServiceForExtension(extension_id);
  service->BindReceiverAndObserver(std::move(receiver), std::move(observer));
}

VpnServiceForExtensionAsh* VpnServiceAsh::GetVpnServiceForExtension(
    const std::string& extension_id) {
  auto& service = extension_id_to_service_[extension_id];
  if (!service) {
    service =
        std::make_unique<VpnServiceForExtensionAsh>(extension_id, controller_);
  }
  return service.get();
}

void VpnServiceAsh::Reset() {
  controller_ = nullptr;
  extension_id_to_service_.clear();
}

}  // namespace crosapi
