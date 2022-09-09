// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_LIFETIME_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_LIFETIME_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class ExtensionService;
class ExtensionSystem;
class ProcessManager;
}  // namespace extensions

namespace ash {

// Manages the lifetime of the login-screen policy-installed extensions and
// apps, making sure that they are stopped during an active user session.
class LoginScreenExtensionsLifetimeManager final
    : public KeyedService,
      public ProfileManagerObserver,
      public session_manager::SessionManagerObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  explicit LoginScreenExtensionsLifetimeManager(
      Profile* signin_original_profile);
  LoginScreenExtensionsLifetimeManager(
      const LoginScreenExtensionsLifetimeManager&) = delete;
  LoginScreenExtensionsLifetimeManager& operator=(
      const LoginScreenExtensionsLifetimeManager&) = delete;
  ~LoginScreenExtensionsLifetimeManager() override;

  // KeyedService:
  void Shutdown() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;

 private:
  bool ShouldEnableLoginScreenPolicyExtensions() const;
  extensions::ExtensionService* GetExtensionService();
  void UpdateStateIfProfileReady();
  void UpdateState();
  extensions::ExtensionIdList GetPolicyExtensionIds() const;
  void DisablePolicyExtensions();
  void EnablePolicyExtensions();
  void DisableExtension(const extensions::ExtensionId& extension_id);

  // Unowned pointers:
  raw_ptr<Profile> const signin_original_profile_;
  raw_ptr<extensions::ExtensionSystem> const extension_system_;
  raw_ptr<extensions::ProcessManager> const extensions_process_manager_;
  raw_ptr<session_manager::SessionManager> const session_manager_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  // Must be the last member.
  base::WeakPtrFactory<LoginScreenExtensionsLifetimeManager> weak_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_LIFETIME_MANAGER_H_
