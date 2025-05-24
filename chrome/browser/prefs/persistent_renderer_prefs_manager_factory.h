// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PERSISTENT_RENDERER_PREFS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PREFS_PERSISTENT_RENDERER_PREFS_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class Profile;

class PersistentRendererPrefsManager;

// Singleton that provides access to Profile specific
// PersistentRendererPrefsManagers.
class PersistentRendererPrefsManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static PersistentRendererPrefsManager* GetForProfile(Profile* profile);

  static PersistentRendererPrefsManagerFactory* GetInstance();

  PersistentRendererPrefsManagerFactory(
      const PersistentRendererPrefsManagerFactory&) = delete;
  PersistentRendererPrefsManagerFactory& operator=(
      const PersistentRendererPrefsManagerFactory&) = delete;

 private:
  friend base::NoDestructor<PersistentRendererPrefsManagerFactory>;

  PersistentRendererPrefsManagerFactory();
  ~PersistentRendererPrefsManagerFactory() override;

  // BrowserContextKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_PREFS_PERSISTENT_RENDERER_PREFS_MANAGER_FACTORY_H_
