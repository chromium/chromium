// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/networking_private_ash.h"

#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/networking_private.mojom-forward.h"
#include "chromeos/crosapi/mojom/networking_private.mojom.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using ::ash::NetworkHandler;
using ::ash::NetworkState;
using ::ash::NetworkStateHandler;

namespace crosapi {

namespace {

extensions::NetworkingPrivateDelegate* GetDelegate() {
  Profile* primary_profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  return extensions::NetworkingPrivateDelegateFactory::GetForBrowserContext(
      primary_profile);
}

// Following are several adapter callbacks which make the mojo callbacks
// compatible with the NetworkingPrivateDelegate callbacks.
//
// Some The crosapi::mojom::NetworkingPrivate calls expect a single callback,
// whereas the API passes in two (one for success and one for failure).
// A failure will be signaled by a non empty string.
//
// See networking_private_lacros.cc for the reverse operation.

// This function splits the callback into a void success call and an error call.
using VoidSuccessOrFailureCallback =
    base::OnceCallback<void(const std::string& error_message)>;

std::pair<extensions::NetworkingPrivateDelegate::VoidCallback,
          extensions::NetworkingPrivateDelegate::FailureCallback>
SplitVoidAdapterCallback(VoidSuccessOrFailureCallback callback) {
  auto [success, failure] = base::SplitOnceCallback(std::move(callback));

  return {base::BindOnce(
              [](VoidSuccessOrFailureCallback callback) {
                std::move(callback).Run("");
              },
              std::move(success)),
          base::BindOnce(
              [](VoidSuccessOrFailureCallback callback,
                 const std::string& error) { std::move(callback).Run(error); },
              std::move(failure))};
}

// This function splits the callback into a success call returning a string and
// an error call.
using StringSuccessOrFailureCallback =
    base::OnceCallback<void(mojom::StringSuccessOrErrorReturnPtr result)>;

std::pair<extensions::NetworkingPrivateDelegate::StringCallback,
          extensions::NetworkingPrivateDelegate::FailureCallback>
SplitStringAdapterCallback(StringSuccessOrFailureCallback callback) {
  auto [success, failure] = base::SplitOnceCallback(std::move(callback));

  return {
      base::BindOnce(
          [](StringSuccessOrFailureCallback callback,
             const std::string& result) {
            std::move(callback).Run(
                mojom::StringSuccessOrErrorReturn::NewSuccessResult(result));
          },
          std::move(success)),
      base::BindOnce(
          [](StringSuccessOrFailureCallback callback,
             const std::string& error) {
            std::move(callback).Run(
                mojom::StringSuccessOrErrorReturn::NewError(error));
          },
          std::move(failure))};
}

// This function splits the callback into a success call returning a Dictionary
// and an error call.
using DictionarySuccessOrFailureCallback =
    base::OnceCallback<void(mojom::DictionarySuccessOrErrorReturnPtr result)>;

std::pair<extensions::NetworkingPrivateDelegate::DictionaryCallback,
          extensions::NetworkingPrivateDelegate::FailureCallback>
SplitDictionaryAdapterCallback(DictionarySuccessOrFailureCallback callback) {
  auto [success, failure] = base::SplitOnceCallback(std::move(callback));

  return {
      base::BindOnce(
          [](DictionarySuccessOrFailureCallback callback,
             base::Value::Dict result) {
            std::move(callback).Run(
                mojom::DictionarySuccessOrErrorReturn::NewSuccessResult(
                    std::move(result)));
          },
          std::move(success)),
      base::BindOnce(
          [](DictionarySuccessOrFailureCallback callback,
             const std::string& error) {
            std::move(callback).Run(
                mojom::DictionarySuccessOrErrorReturn::NewError(error));
          },
          std::move(failure))};
}

// This function splits the callback into a success call returning a list and
// an error call.
using ListValueSuccessOrFailureCallback =
    base::OnceCallback<void(mojom::ListValueSuccessOrErrorReturnPtr)>;

std::pair<base::OnceCallback<void(base::Value::List)>,
          extensions::NetworkingPrivateDelegate::FailureCallback>
SplitListValueAdapterCallback(ListValueSuccessOrFailureCallback callback) {
  auto [success, failure] = base::SplitOnceCallback(std::move(callback));

  return {base::BindOnce(
              [](ListValueSuccessOrFailureCallback callback,
                 base::Value::List result) {
                std::move(callback).Run(
                    mojom::ListValueSuccessOrErrorReturn::NewSuccessResult(
                        std::move(result)));
              },
              std::move(success)),
          base::BindOnce(
              [](ListValueSuccessOrFailureCallback callback,
                 const std::string& error) {
                std::move(callback).Run(
                    mojom::ListValueSuccessOrErrorReturn::NewError(error));
              },
              std::move(failure))};
}

// This adapter will handle the case where a list get returned.
using ValueListMojoCallback =
    base::OnceCallback<void(std::optional<base::Value::List>)>;
using ValueListDelegateCallback =
    base::OnceCallback<void(base::Value::List result)>;
ValueListDelegateCallback ValueListAdapterCallback(
    ValueListMojoCallback result_callback) {
  return base::BindOnce(
      [](ValueListMojoCallback callback, base::Value::List result) {
        std::move(callback).Run(std::move(result));
      },
      std::move(result_callback));
}

// This adapter will handle the properties call back to Lacros using the mojo
// api. It assumes that there can be either a result - or an error, but not
// both.
using PropertiesMojoCallback =
    base::OnceCallback<void(mojom::PropertiesSuccessOrErrorReturnPtr result)>;
using PropertiesDelegateCallback =
    base::OnceCallback<void(std::optional<::base::Value::Dict> result,
                            const std::optional<std::string>& error)>;

PropertiesDelegateCallback PropertiesAdapterCallback(
    PropertiesMojoCallback result_callback) {
  return base::BindOnce(
      [](PropertiesMojoCallback callback,
         std::optional<::base::Value::Dict> result,
         const std::optional<std::string>& error) {
        if (result) {
          std::move(callback).Run(
              mojom::PropertiesSuccessOrErrorReturn::NewSuccessResult(
                  base::Value(std::move(*result))));
        } else {
          std::move(callback).Run(
              mojom::PropertiesSuccessOrErrorReturn::NewError(*error));
        }
      },
      std::move(result_callback));
}

void DeviceStateListCallbackAdapter(
    NetworkingPrivateAsh::GetDeviceStateListCallback callback,
    std::optional<extensions::NetworkingPrivateDelegate::DeviceStateList>
        result) {
  if (!result) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::vector<std::optional<base::Value::Dict>> list;

  for (size_t i = 0; i < result->size(); ++i) {
    list.emplace_back(result->at(i).ToValue());
  }

  std::move(callback).Run(std::move(list));
}

mojom::CaptivePortalStatus GetCaptivePortalStatusFromNetworkState(
    const NetworkState* network) {
  if (!network) {
    return mojom::CaptivePortalStatus::kUnknown;
  }
  if (!network->IsConnectedState()) {
    return mojom::CaptivePortalStatus::kOnline;
  }

  switch (network->GetPortalState()) {
    case NetworkState::PortalState::kUnknown:
      return mojom::CaptivePortalStatus::kUnknown;
    case NetworkState::PortalState::kOnline:
      return mojom::CaptivePortalStatus::kOnline;
    case NetworkState::PortalState::kPortalSuspected:
    case NetworkState::PortalState::kPortal:
    case NetworkState::PortalState::kNoInternet:
      return mojom::CaptivePortalStatus::kPortal;
  }
}

}  // namespace

NetworkingPrivateAsh::NetworkingPrivateAsh() {
  observers_.set_disconnect_handler(base::BindRepeating(
      &NetworkingPrivateAsh::OnObserverDisconnected, base::Unretained(this)));
}

NetworkingPrivateAsh::~NetworkingPrivateAsh() = default;

void NetworkingPrivateAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkingPrivate> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void NetworkingPrivateAsh::GetProperties(const std::string& guid,
                                         GetPropertiesCallback callback) {
  GetDelegate()->GetProperties(guid,
                               PropertiesAdapterCallback(std::move(callback)));
}

void NetworkingPrivateAsh::GetManagedProperties(
    const std::string& guid,
    GetManagedPropertiesCallback callback) {
  GetDelegate()->GetManagedProperties(
      guid, PropertiesAdapterCallback(std::move(callback)));
}

void NetworkingPrivateAsh::GetState(const std::string& guid,
                                    GetStateCallback callback) {
  auto [success, failure] = SplitDictionaryAdapterCallback(std::move(callback));
  GetDelegate()->GetState(guid, std::move(success), std::move(failure));
}

void NetworkingPrivateAsh::SetProperties(const std::string& guid,
                                         base::Value::Dict properties,
                                         bool allow_set_shared_config,
                                         SetPropertiesCallback callback) {
  auto [success, failure] = SplitVoidAdapterCallback(std::move(callback));
  GetDelegate()->SetProperties(guid, std::move(properties),
                               allow_set_shared_config, std::move(success),
                               std::move(failure));
}

void NetworkingPrivateAsh::CreateNetwork(bool shared,
                                         base::Value properties,
                                         CreateNetworkCallback callback) {
  auto [success, failure] = SplitStringAdapterCallback(std::move(callback));
  GetDelegate()->CreateNetwork(shared, std::move(properties).TakeDict(),
                               std::move(success), std::move(failure));
}

void NetworkingPrivateAsh::ForgetNetwork(const std::string& guid,
                                         bool allow_forget_shared_config,
                                         ForgetNetworkCallback callback) {
  auto [success, failure] = SplitVoidAdapterCallback(std::move(callback));
  GetDelegate()->ForgetNetwork(guid, allow_forget_shared_config,
                               std::move(success), std::move(failure));
}

void NetworkingPrivateAsh::GetNetworks(const std::string& network_type,
                                       bool configured_only,
                                       bool visible_only,
                                       int limit,
                                       GetNetworksCallback callback) {
  auto [success, failure] = SplitListValueAdapterCallback(std::move(callback));
  GetDelegate()->GetNetworks(network_type, configured_only, visible_only, limit,
                             std::move(success), std::move(failure));
}

void NetworkingPrivateAsh::StartConnect(const std::string& guid,
                                        StartConnectCallback callback) {
  auto [success, failure] = SplitVoidAdapterCallback(std::move(callback));
  GetDelegate()->StartConnect(guid, std::move(success), std::move(failure));
}

void NetworkingPrivateAsh::StartDisconnect(const std::string& guid,
                                           StartDisconnectCallback callback) {
  auto [success, failure] = SplitVoidAdapterCallback(std::move(callback));
  GetDelegate()->StartDisconnect(guid, std::move(success), std::move(failure));
}

void NetworkingPrivateAsh::StartActivate(const std::string& guid,
                                         const std::string& carrier,
                                         StartActivateCallback callback) {
  auto [success, failure] = SplitVoidAdapterCallback(std::move(callback));
  GetDelegate()->StartActivate(guid, carrier, std::move(success),
                               std::move(failure));
}

void NetworkingPrivateAsh::GetCaptivePortalStatus(
    const std::string& guid,
    GetCaptivePortalStatusCallback callback) {
  auto [success, failure] = SplitStringAdapterCallback(std::move(callback));
  GetDelegate()->GetCaptivePortalStatus(guid, std::move(success),
                                        std::move(failure));
}

void NetworkingPrivateAsh::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    UnlockCellularSimCallback callback) {
  auto [success, failure] = SplitVoidAdapterCallback(std::move(callback));
  GetDelegate()->UnlockCellularSim(guid, pin, puk, std::move(success),
                                   std::move(failure));
}

void NetworkingPrivateAsh::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    SetCellularSimStateCallback callback) {
  auto [success, failure] = SplitVoidAdapterCallback(std::move(callback));
  GetDelegate()->SetCellularSimState(guid, require_pin, current_pin, new_pin,
                                     std::move(success), std::move(failure));
}

void NetworkingPrivateAsh::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    SelectCellularMobileNetworkCallback callback) {
  auto [success, failure] = SplitVoidAdapterCallback(std::move(callback));
  GetDelegate()->SelectCellularMobileNetwork(
      guid, network_id, std::move(success), std::move(failure));
}

void NetworkingPrivateAsh::GetEnabledNetworkTypes(
    GetEnabledNetworkTypesCallback callback) {
  GetDelegate()->GetEnabledNetworkTypes(
      ValueListAdapterCallback(std::move(callback)));
}

void NetworkingPrivateAsh::GetDeviceStateList(
    GetDeviceStateListCallback callback) {
  GetDelegate()->GetDeviceStateList(
      base::BindOnce(&DeviceStateListCallbackAdapter, std::move(callback)));
}

void NetworkingPrivateAsh::GetGlobalPolicy(GetGlobalPolicyCallback callback) {
  GetDelegate()->GetGlobalPolicy(std::move(callback));
}

void NetworkingPrivateAsh::GetCertificateLists(
    GetCertificateListsCallback callback) {
  GetDelegate()->GetCertificateLists(std::move(callback));
}

void NetworkingPrivateAsh::EnableNetworkType(
    const std::string& type,
    EnableNetworkTypeCallback callback) {
  GetDelegate()->EnableNetworkType(type, std::move(callback));
}

void NetworkingPrivateAsh::DisableNetworkType(
    const std::string& type,
    DisableNetworkTypeCallback callback) {
  GetDelegate()->DisableNetworkType(type, std::move(callback));
}

void NetworkingPrivateAsh::RequestScan(const std::string& type,
                                       RequestScanCallback callback) {
  GetDelegate()->RequestScan(type, std::move(callback));
}

void NetworkingPrivateAsh::AddObserver(
    mojo::PendingRemote<mojom::NetworkingPrivateDelegateObserver> observer) {
  const bool is_listening_network_state = !observers_.empty();

  observers_.Add(mojo::Remote<mojom::NetworkingPrivateDelegateObserver>(
      std::move(observer)));

  if (!is_listening_network_state) {
    auto* net_handler = NetworkHandler::Get();
    network_state_observation_.Observe(net_handler->network_state_handler());
    network_certificate_observation_.Observe(
        net_handler->network_certificate_handler());
  }
}

void NetworkingPrivateAsh::DeviceListChanged() {
  for (auto& observer : observers_) {
    observer->OnDeviceStateListChanged();
  }
}

void NetworkingPrivateAsh::DevicePropertiesUpdated(
    const ash::DeviceState* device) {
  // networkingPrivate uses a single event for device changes.
  DeviceListChanged();

  // DeviceState changes may affect Cellular networks.
  if (device->type() != shill::kTypeCellular)
    return;

  NetworkStateHandler::NetworkStateList cellular_networks;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      ash::NetworkTypePattern::Cellular(), false /* configured_only */,
      true /* visible_only */, -1 /* default limit */, &cellular_networks);
  for (const NetworkState* network : cellular_networks) {
    NetworkPropertiesUpdated(network);
  }
}

void NetworkingPrivateAsh::NetworkListChanged() {
  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkList(
      &networks);
  std::vector<std::string> changes;
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin();
       iter != networks.end(); ++iter) {
    changes.push_back((*iter)->guid());
  }

  for (auto& observer : observers_) {
    observer->OnNetworkListChangedEvent(changes);
  }
}

void NetworkingPrivateAsh::NetworkPropertiesUpdated(
    const NetworkState* network) {
  for (auto& observer : observers_) {
    observer->OnNetworksChangedEvent(
        std::vector<std::string>(1, network->guid()));
  }
}

void NetworkingPrivateAsh::PortalStateChanged(
    const NetworkState* network,
    NetworkState::PortalState portal_state) {
  const std::string guid = network ? network->guid() : std::string();
  const mojom::CaptivePortalStatus status =
      GetCaptivePortalStatusFromNetworkState(network);

  for (auto& observer : observers_) {
    observer->OnPortalDetectionCompleted(guid, status);
  }
}

void NetworkingPrivateAsh::OnCertificatesChanged() {
  for (auto& observer : observers_) {
    observer->OnCertificateListsChanged();
  }
}

void NetworkingPrivateAsh::OnObserverDisconnected(mojo::RemoteSetElementId id) {
  if (observers_.empty()) {
    network_state_observation_.Reset();
    network_certificate_observation_.Reset();
  }
}

}  // namespace crosapi
