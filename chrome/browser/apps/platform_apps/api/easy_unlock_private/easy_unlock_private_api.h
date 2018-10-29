// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

// Implementations for chrome.easyUnlockPrivate API functions.

namespace base {
class OneShotTimer;
}

namespace content {
class BrowserContext;
}

namespace cryptauth {
class Connection;
}

namespace proximity_auth {
class BluetoothLowEnergyConnectionFinder;
}

namespace chrome_apps {
namespace api {

class EasyUnlockPrivateConnectionManager;

class EasyUnlockPrivateAPI : public extensions::BrowserContextKeyedAPI {
 public:
  using Factory =
      extensions::BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>;
  static Factory* GetFactoryInstance();

  static const bool kServiceRedirectedInIncognito = true;

  explicit EasyUnlockPrivateAPI(content::BrowserContext* context);
  ~EasyUnlockPrivateAPI() override;

  EasyUnlockPrivateConnectionManager* get_connection_manager() {
    return connection_manager_.get();
  }

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "EasyUnlockPrivate"; }

  // KeyedService implementation.
  void Shutdown() override;

  std::unique_ptr<EasyUnlockPrivateConnectionManager> connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateAPI);
};

class EasyUnlockPrivateGetStringsFunction : public UIThreadExtensionFunction {
 public:
  EasyUnlockPrivateGetStringsFunction();

 protected:
  ~EasyUnlockPrivateGetStringsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getStrings",
                             EASYUNLOCKPRIVATE_GETSTRINGS)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetStringsFunction);
};

class EasyUnlockPrivateShowErrorBubbleFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.showErrorBubble",
                             EASYUNLOCKPRIVATE_SHOWERRORBUBBLE)
  EasyUnlockPrivateShowErrorBubbleFunction();

 private:
  ~EasyUnlockPrivateShowErrorBubbleFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateShowErrorBubbleFunction);
};

class EasyUnlockPrivateHideErrorBubbleFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.hideErrorBubble",
                             EASYUNLOCKPRIVATE_HIDEERRORBUBBLE)
  EasyUnlockPrivateHideErrorBubbleFunction();

 private:
  ~EasyUnlockPrivateHideErrorBubbleFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateHideErrorBubbleFunction);
};

class EasyUnlockPrivateFindSetupConnectionFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.findSetupConnection",
                             EASYUNLOCKPRIVATE_FINDSETUPCONNECTION)
  EasyUnlockPrivateFindSetupConnectionFunction();

 private:
  ~EasyUnlockPrivateFindSetupConnectionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Called when the connection with the remote device advertising the setup
  // service was found.
  void OnConnectionFound(std::unique_ptr<cryptauth::Connection> connection);

  // Callback when waiting for |connection_finder_| to return.
  void OnConnectionFinderTimedOut();

  // The BLE connection finder instance.
  std::unique_ptr<proximity_auth::BluetoothLowEnergyConnectionFinder>
      connection_finder_;

  // Used for timing out when waiting for the connection finder to return.
  std::unique_ptr<base::OneShotTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateFindSetupConnectionFunction);
};

class EasyUnlockPrivateSetupConnectionDisconnectFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setupConnectionDisconnect",
                             EASYUNLOCKPRIVATE_SETUPCONNECTIONDISCONNECT)
  EasyUnlockPrivateSetupConnectionDisconnectFunction();

 private:
  ~EasyUnlockPrivateSetupConnectionDisconnectFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetupConnectionDisconnectFunction);
};

class EasyUnlockPrivateSetupConnectionSendFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setupConnectionSend",
                             EASYUNLOCKPRIVATE_SETUPCONNECTIONSEND)
  EasyUnlockPrivateSetupConnectionSendFunction();

 private:
  ~EasyUnlockPrivateSetupConnectionSendFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetupConnectionSendFunction);
};

}  // namespace api
}  // namespace chrome_apps

template <>
void chrome_apps::api::EasyUnlockPrivateAPI::Factory::
    DeclareFactoryDependencies();

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
