// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GCM_APP_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GCM_APP_HANDLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace gcm {
class GCMDriver;
}
namespace instance_id {
class InstanceIDDriver;
}

namespace extensions {

class GcmJsEventRouter;

// Defines the interface to provide handling logic for a given app.
class ExtensionGCMAppHandler : public gcm::GCMAppHandler,
                               public BrowserContextKeyedAPI,
                               public ExtensionRegistryObserver {
 public:
  explicit ExtensionGCMAppHandler(content::BrowserContext* context);
  ~ExtensionGCMAppHandler() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ExtensionGCMAppHandler>*
  GetFactoryInstance();
  void Shutdown() override;

  // gcm::GCMAppHandler implementation.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

 protected:
  // Could be overridden by testing purpose.
  virtual void OnUnregisterCompleted(const std::string& app_id,
                                     gcm::GCMClient::Result result);
  virtual void OnDeleteIDCompleted(const std::string& app_id,
                                   instance_id::InstanceID::Result result);
  virtual void AddAppHandler(const std::string& app_id);
  virtual void RemoveAppHandler(const std::string& app_id);

  gcm::GCMDriver* GetGCMDriver() const;
  instance_id::InstanceIDDriver* GetInstanceIDDriver() const;

 private:
  friend class BrowserContextKeyedAPIFactory<ExtensionGCMAppHandler>;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  void RemoveInstanceID(const std::string& app_id);
  void AddDummyAppHandler();
  void RemoveDummyAppHandler();

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ExtensionGCMAppHandler"; }
  static const bool kServiceIsNULLWhileTesting = true;

  Profile* profile_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  std::unique_ptr<extensions::GcmJsEventRouter> js_event_router_;

  base::WeakPtrFactory<ExtensionGCMAppHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionGCMAppHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GCM_APP_HANDLER_H_
