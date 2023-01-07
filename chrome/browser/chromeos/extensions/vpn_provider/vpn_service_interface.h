// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_INTERFACE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {

class VpnServiceProxy;

}  // namespace content

namespace extensions::api {

// This class is the basic interface for chrome.vpnProvider extension API
// methods.
class VpnServiceInterface : public KeyedService {
 public:
  using SuccessCallback = base::OnceClosure;
  using FailureCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  // Sends UIEvent.showAddDialog to the extension with |extension_id|.
  virtual void SendShowAddDialogToExtension(
      const std::string& extension_id) = 0;

  // Sends UIEvent.showConfigureDialog for |configuration_name| to
  // the extension with |extension_id|.
  virtual void SendShowConfigureDialogToExtension(
      const std::string& extension_id,
      const std::string& configuration_name) = 0;

  // Creates a new VPN configuration with |configuration_name| as the name and
  // attaches it to the extension with id |extension_id|.
  // Calls |success| or |failure| based on the outcome.
  virtual void CreateConfiguration(const std::string& extension_id,
                                   const std::string& configuration_name,
                                   SuccessCallback,
                                   FailureCallback) = 0;

  // Destroys the VPN configuration with |configuration_name| after verifying
  // that it belongs to the extension with id |extension_id|. Calls |success| or
  // |failure| based on the outcome.
  virtual void DestroyConfiguration(const std::string& extension_id,
                                    const std::string& configuration_name,
                                    SuccessCallback,
                                    FailureCallback) = 0;

  // Set |parameters| for the active VPN configuration after verifying that it
  // belongs to the extension with id |extension_id|.
  // Calls |success| or |failure| based on the outcome.
  virtual void SetParameters(const std::string& extension_id,
                             base::Value::Dict parameters,
                             SuccessCallback,
                             FailureCallback) = 0;

  // Sends an IP packet contained in |data| to the active VPN configuration
  // after verifying that it belongs to the extension with id |extension_id|.
  // Calls |success| or |failure| based on the outcome.
  virtual void SendPacket(const std::string& extension_id,
                          const std::vector<char>& data,
                          SuccessCallback,
                          FailureCallback) = 0;

  // Notifies new connection state to the active VPN configuration after
  // verifying that it belongs to the extension with id |extension_id|.
  // Calls |success| or |failure| based on the outcome.
  virtual void NotifyConnectionStateChanged(const std::string& extension_id,
                                            bool connection_success,
                                            SuccessCallback,
                                            FailureCallback) = 0;

  // Returns a VpnServiceProxy that is used by Pepper API.
  virtual std::unique_ptr<content::VpnServiceProxy> GetVpnServiceProxy() = 0;
};

}  // namespace extensions::api

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_INTERFACE_H_
