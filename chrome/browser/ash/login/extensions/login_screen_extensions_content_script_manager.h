// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_CONTENT_SCRIPT_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_CONTENT_SCRIPT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class ExtensionService;
class ExtensionSystem;
}  // namespace extensions

namespace ash {

// Verifies the content script login screen extensions set, and disables the
// extensions that load content scripts matching non-allowlisted URLs.
class LoginScreenExtensionsContentScriptManager final
    : public KeyedService,
      public extensions::ExtensionRegistryObserver {
 public:
  explicit LoginScreenExtensionsContentScriptManager(
      Profile* signin_original_profile);

  LoginScreenExtensionsContentScriptManager(
      const LoginScreenExtensionsContentScriptManager&) = delete;
  LoginScreenExtensionsContentScriptManager& operator=(
      const LoginScreenExtensionsContentScriptManager&) = delete;
  ~LoginScreenExtensionsContentScriptManager() override;

  // KeyedService:
  void Shutdown() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;

 private:
  extensions::ExtensionService* GetExtensionService();
  void DisableExtension(const extensions::ExtensionId& extension_id);

  // Unowned pointers:
  raw_ptr<Profile> const signin_original_profile_;
  raw_ptr<extensions::ExtensionSystem> const extension_system_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // Must be the last member.
  base::WeakPtrFactory<LoginScreenExtensionsContentScriptManager> weak_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_CONTENT_SCRIPT_MANAGER_H_
