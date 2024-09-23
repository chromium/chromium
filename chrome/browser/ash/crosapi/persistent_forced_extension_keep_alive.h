// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PERSISTENT_FORCED_EXTENSION_KEEP_ALIVE_H_
#define CHROME_BROWSER_ASH_CROSAPI_PERSISTENT_FORCED_EXTENSION_KEEP_ALIVE_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/crosapi/browser_manager_scoped_keep_alive.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"

class PrefChangeRegistrar;

namespace content {
class BrowserContext;
}

namespace crosapi {

// Registers a KeepAlive instance in Lacros if there is an extension
// force-installed by admin policy that should always be running.
class PersistentForcedExtensionKeepAlive final : public KeyedService {
 public:
  explicit PersistentForcedExtensionKeepAlive(PrefService* user_prefs);
  PersistentForcedExtensionKeepAlive(
      const PersistentForcedExtensionKeepAlive&) = delete;
  PersistentForcedExtensionKeepAlive& operator=(
      const PersistentForcedExtensionKeepAlive&) = delete;
  ~PersistentForcedExtensionKeepAlive() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // Reads the `kInstallForceList` pref value and, if required, updates
  // keep_alive_. Called when the user prefs changed.
  void UpdateKeepAlive();

  std::unique_ptr<BrowserManagerScopedKeepAlive> keep_alive_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Must be the last member.
  base::WeakPtrFactory<PersistentForcedExtensionKeepAlive> weak_factory_{this};
};

// Factory for the `PersistentForcedExtensionKeepAlive` KeyedService.
class PersistentForcedExtensionKeepAliveFactory
    : public ProfileKeyedServiceFactory {
 public:
  static PersistentForcedExtensionKeepAliveFactory* GetInstance();

 private:
  friend class base::NoDestructor<PersistentForcedExtensionKeepAliveFactory>;

  PersistentForcedExtensionKeepAliveFactory();
  ~PersistentForcedExtensionKeepAliveFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_PERSISTENT_FORCED_EXTENSION_KEEP_ALIVE_H_
