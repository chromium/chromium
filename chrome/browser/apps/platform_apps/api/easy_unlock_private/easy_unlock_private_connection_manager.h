// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "chrome/browser/apps/platform_apps/api/easy_unlock_private/easy_unlock_private_connection.h"
#include "chrome/common/apps/platform_apps/api/easy_unlock_private.h"
#include "components/cryptauth/connection.h"
#include "components/cryptauth/connection_observer.h"
#include "components/cryptauth/wire_message.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
}

namespace chrome_apps {
namespace api {

// EasyUnlockPrivateConnectionManager is used by the EasyUnlockPrivateAPI to
// interface with cryptauth::Connection.
class EasyUnlockPrivateConnectionManager
    : public cryptauth::ConnectionObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  explicit EasyUnlockPrivateConnectionManager(content::BrowserContext* context);
  ~EasyUnlockPrivateConnectionManager() override;

  // Stores |connection| in the API connection manager. Returns the
  // |connection_id|.
  int AddConnection(const extensions::Extension* extension,
                    std::unique_ptr<cryptauth::Connection> connection,
                    bool persistent);

  // Returns the status of the connection with |connection_id|.
  easy_unlock_private::ConnectionStatus ConnectionStatus(
      const extensions::Extension* extension,
      int connection_id) const;

  // Disconnects the connection with |connection_id|. Returns true if
  // |connection_id| is valid.
  bool Disconnect(const extensions::Extension* extension, int connection_id);

  // Sends |message_body| through the connection with |connection_id|. Returns
  // true if |connection_id| is valid.
  bool SendMessage(const extensions::Extension* extension,
                   int connection_id,
                   const std::string& message_body);

  // Returns the Bluetooth address of the device connected with a given
  // |connection_id|, and an empty string if |connection_id| was not found.
  std::string GetDeviceAddress(const extensions::Extension* extension,
                               int connection_id) const;

  // cryptauth::ConnectionObserver:
  void OnConnectionStatusChanged(
      cryptauth::Connection* connection,
      cryptauth::Connection::Status old_status,
      cryptauth::Connection::Status new_status) override;
  void OnMessageReceived(const cryptauth::Connection& connection,
                         const cryptauth::WireMessage& message) override;
  void OnSendCompleted(const cryptauth::Connection& connection,
                       const cryptauth::WireMessage& message,
                       bool success) override;

 private:
  // extensions::ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // Dispatches |event_name| with |args| to all listeners. Retrieves the
  // |connection_id| corresponding to the event and rewrite the first argument
  // in |args| with it.
  void DispatchConnectionEvent(
      const std::string& event_name,
      extensions::events::HistogramValue histogram_value,
      const cryptauth::Connection* connection,
      std::unique_ptr<base::ListValue> args);

  // Convenience method to get the API resource manager.
  EasyUnlockPrivateConnectionResourceManager* GetResourceManager() const;

  // Convenience method to get the connection with |connection_id| created by
  // extension with |extension_id| from the API resource manager.
  cryptauth::Connection* GetConnection(const std::string& extension_id,
                                       int connection_id) const;

  // Find the connection_id for |connection| owned by |extension_id| from the
  // API resource manager.
  int FindConnectionId(const std::string& extension_id,
                       const cryptauth::Connection* connection);

  // BrowserContext passed during initialization.
  content::BrowserContext* browser_context_;

  // The set of extensions that have at least one connection.
  std::set<std::string> extensions_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateConnectionManager);
};

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_MANAGER_H_
