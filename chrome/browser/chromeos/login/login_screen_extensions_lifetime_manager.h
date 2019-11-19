// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_SCREEN_EXTENSIONS_LIFETIME_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_SCREEN_EXTENSIONS_LIFETIME_MANAGER_H_

#include "components/session_manager/core/session_manager_observer.h"
#include "extensions/browser/extension_registry_observer.h"

namespace chromeos {

// Manages the lifetime of the login-screen policy-installed extensions and
// apps, making sure that they are stopped during an active user session.
class LoginScreenExtensionsLifetimeManager final
    : public session_manager::SessionManagerObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  LoginScreenExtensionsLifetimeManager();
  LoginScreenExtensionsLifetimeManager(
      const LoginScreenExtensionsLifetimeManager&) = delete;
  LoginScreenExtensionsLifetimeManager& operator=(
      const LoginScreenExtensionsLifetimeManager&) = delete;
  ~LoginScreenExtensionsLifetimeManager() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_SCREEN_EXTENSIONS_LIFETIME_MANAGER_H_
