// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_networking_attributes/enterprise_networking_attributes_api.h"

#include <optional>

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_networking_attributes.h"
#include "net/base/ip_address.h"

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/networking_attributes_ash.h"

namespace extensions {

EnterpriseNetworkingAttributesGetNetworkDetailsFunction::
    EnterpriseNetworkingAttributesGetNetworkDetailsFunction() = default;

EnterpriseNetworkingAttributesGetNetworkDetailsFunction::
    ~EnterpriseNetworkingAttributesGetNetworkDetailsFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseNetworkingAttributesGetNetworkDetailsFunction::Run() {
  auto callback = base::BindOnce(
      &EnterpriseNetworkingAttributesGetNetworkDetailsFunction::OnResult, this);

  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->networking_attributes_ash()
      ->GetNetworkDetails(std::move(callback));

  return RespondLater();
}

void EnterpriseNetworkingAttributesGetNetworkDetailsFunction::OnResult(
    crosapi::mojom::GetNetworkDetailsResultPtr result) {
  using Result = crosapi::mojom::GetNetworkDetailsResult;
  switch (result->which()) {
    case Result::Tag::kErrorMessage:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::kNetworkDetails:
      api::enterprise_networking_attributes::NetworkDetails network_details;

      std::optional<net::IPAddress> ipv4_address =
          result->get_network_details()->ipv4_address;
      std::optional<net::IPAddress> ipv6_address =
          result->get_network_details()->ipv6_address;

      network_details.mac_address = result->get_network_details()->mac_address;
      if (ipv4_address.has_value()) {
        network_details.ipv4 = ipv4_address->ToString();
      }
      if (ipv6_address.has_value()) {
        network_details.ipv6 = ipv6_address->ToString();
      }

      Respond(WithArguments(network_details.ToValue()));
      return;
  }
}

}  // namespace extensions
