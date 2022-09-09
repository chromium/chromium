// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_PROVIDER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_PROVIDER_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace extensions {

class VpnThreadExtensionFunction : public ExtensionFunction {
 public:
  void SignalCallCompletionSuccess();
  void SignalCallCompletionSuccessWithId(const std::string& configuration_name);
  void SignalCallCompletionFailure(const std::string& error_name,
                                   const std::string& error_message);

 protected:
  ~VpnThreadExtensionFunction() override;
};

class VpnProviderCreateConfigFunction : public VpnThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.createConfig",
                             VPNPROVIDER_CREATECONFIG)

 protected:
  ~VpnProviderCreateConfigFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class VpnProviderDestroyConfigFunction : public VpnThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.destroyConfig",
                             VPNPROVIDER_DESTROYCONFIG)

 protected:
  ~VpnProviderDestroyConfigFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class VpnProviderSetParametersFunction : public VpnThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.setParameters",
                             VPNPROVIDER_SETPARAMETERS)

 protected:
  ~VpnProviderSetParametersFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class VpnProviderSendPacketFunction : public VpnThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.sendPacket", VPNPROVIDER_SENDPACKET)

 protected:
  ~VpnProviderSendPacketFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class VpnProviderNotifyConnectionStateChangedFunction
    : public VpnThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vpnProvider.notifyConnectionStateChanged",
                             VPNPROVIDER_NOTIFYCONNECTIONSTATECHANGED)

 protected:
  ~VpnProviderNotifyConnectionStateChangedFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_PROVIDER_API_H_
