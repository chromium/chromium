// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_EXTENSION_KEY_PERMISSIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_EXTENSION_KEY_PERMISSIONS_SERVICE_FACTORY_H_

#include "base/callback_forward.h"
#include "memory"

#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace chromeos {
namespace platform_keys {

class ExtensionKeyPermissionsService;
class KeyPermissionsService;

using GetExtensionKeyPermissionsServiceCallback =
    base::OnceCallback<void(std::unique_ptr<ExtensionKeyPermissionsService>
                                extension_key_permissions_service)>;

// ExtensionKeyPermissionsServiceFactory can be used for retrieving
// ExtensionKeyPermissionsService instances for a specific (Profile, Extension)
// pair.
//
// Note: The underlying KeyPermissionsService instance used by a provided
// ExtensionKeyPermissionsService is only valid during the lifetime of the given
// BrowserContext, i.e., it is the responsibility of the service consumer to
// make sure that the BrowserContext is valid during the lifetime of the
// service.
class ExtensionKeyPermissionsServiceFactory {
 public:
  // |context| and |key_permissions_service| must not be nullptr and must
  // outlive the provided ExtensionKeyPermissionsService instance.
  // |key_permissions_service| must be a KeyPermissionsService instance
  // corresponding to |context|.
  static void GetForBrowserContextAndExtension(
      GetExtensionKeyPermissionsServiceCallback callback,
      content::BrowserContext* context,
      extensions::ExtensionId extension_id,
      KeyPermissionsService* key_permissions_service);

 private:
  ExtensionKeyPermissionsServiceFactory();
  ~ExtensionKeyPermissionsServiceFactory();
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_EXTENSION_KEY_PERMISSIONS_SERVICE_FACTORY_H_
