// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace cryptauth {
class Connection;
}  // namespace cryptauth

namespace chrome_apps {
namespace api {

// An ApiResource wrapper for a cryptauth::Connection.
class EasyUnlockPrivateConnection : public extensions::ApiResource {
 public:
  EasyUnlockPrivateConnection(
      bool persistent,
      const std::string& owner_extension_id,
      std::unique_ptr<cryptauth::Connection> connection);
  ~EasyUnlockPrivateConnection() override;

  // Returns a pointer to the underlying connection object.
  cryptauth::Connection* GetConnection() const;

  // ApiResource override.
  bool IsPersistent() const override;

  // This resource should be managed on the UI thread.
  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  friend class extensions::ApiResourceManager<EasyUnlockPrivateConnection>;
  static const char* service_name() {
    return "EasyUnlockPrivateConnectionManager";
  }

  // True, if this resource should be persistent.
  bool persistent_;

  // The connection is owned by this instance and will automatically disconnect
  // when deleted.
  std::unique_ptr<cryptauth::Connection> connection_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateConnection);
};

}  // namespace api
}  // namespace chrome_apps

using EasyUnlockPrivateConnectionResourceManager =
    extensions::ApiResourceManager<
        chrome_apps::api::EasyUnlockPrivateConnection>;
using EasyUnlockPrivateConnectionResourceManagerFactory =
    extensions::BrowserContextKeyedAPIFactory<
        EasyUnlockPrivateConnectionResourceManager>;

template <>
EasyUnlockPrivateConnectionResourceManagerFactory*
EasyUnlockPrivateConnectionResourceManager::GetFactoryInstance();

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_H_
