// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/easy_unlock_private/easy_unlock_private_connection.h"

#include "base/lazy_instance.h"
#include "components/cryptauth/connection.h"

namespace chrome_apps {
namespace api {

static base::LazyInstance<EasyUnlockPrivateConnectionResourceManagerFactory>::
    DestructorAtExit g_easy_unlock_private_connection_factory =
        LAZY_INSTANCE_INITIALIZER;

EasyUnlockPrivateConnection::EasyUnlockPrivateConnection(
    bool persistent,
    const std::string& owner_extension_id,
    std::unique_ptr<cryptauth::Connection> connection)
    : ApiResource(owner_extension_id),
      persistent_(persistent),
      connection_(connection.release()) {}

EasyUnlockPrivateConnection::~EasyUnlockPrivateConnection() {}

cryptauth::Connection* EasyUnlockPrivateConnection::GetConnection() const {
  return connection_.get();
}

bool EasyUnlockPrivateConnection::IsPersistent() const {
  return persistent_;
}

}  // namespace api
}  // namespace chrome_apps

template <>
EasyUnlockPrivateConnectionResourceManagerFactory*
EasyUnlockPrivateConnectionResourceManager::GetFactoryInstance() {
  return chrome_apps::api::g_easy_unlock_private_connection_factory.Pointer();
}
