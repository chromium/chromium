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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/networking_attributes_ash.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
namespace {

const char kUnsupportedByAsh[] = "Not implemented.";
const char kUnsupportedProfile[] = "Not available for this profile.";

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or empty string on success. |context| is the browser context in which
// the extension is hosted.
std::string ValidateCrosapi(content::BrowserContext* context) {
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::NetworkingAttributes>()) {
    return kUnsupportedByAsh;
  }

  // These APIs are used in security-sensitive contexts. We need to ensure that
  // the user for ash is the same as the user for lacros. We do this by
  // restricting the API to the default profile, which is guaranteed to be the
  // same user.
  if (!Profile::FromBrowserContext(context)->IsMainProfile())
    return kUnsupportedProfile;

  return "";
}

}  // namespace
#endif

namespace extensions {

EnterpriseNetworkingAttributesGetNetworkDetailsFunction::
    EnterpriseNetworkingAttributesGetNetworkDetailsFunction() = default;

EnterpriseNetworkingAttributesGetNetworkDetailsFunction::
    ~EnterpriseNetworkingAttributesGetNetworkDetailsFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseNetworkingAttributesGetNetworkDetailsFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::string error = ValidateCrosapi(browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }
#endif
  auto callback = base::BindOnce(
      &EnterpriseNetworkingAttributesGetNetworkDetailsFunction::OnResult, this);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::NetworkingAttributes>()
      ->GetNetworkDetails(std::move(callback));
#else
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->networking_attributes_ash()
      ->GetNetworkDetails(std::move(callback));
#endif
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
