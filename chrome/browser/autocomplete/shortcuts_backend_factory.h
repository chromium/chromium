// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_BACKEND_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_BACKEND_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

class Profile;

class ShortcutsBackend;

// Singleton that owns all instances of ShortcutsBackend and associates them
// with Profiles.
class ShortcutsBackendFactory : public RefcountedProfileKeyedServiceFactory {
 public:
  static scoped_refptr<ShortcutsBackend> GetForProfile(Profile* profile);

  static scoped_refptr<ShortcutsBackend> GetForProfileIfExists(
      Profile* profile);

  static ShortcutsBackendFactory* GetInstance();

  // Creates and returns a backend for testing purposes.
  static scoped_refptr<RefcountedKeyedService> BuildProfileForTesting(
      content::BrowserContext* profile);

  // Creates and returns a backend but without creating its persistent database
  // for testing purposes.
  static scoped_refptr<RefcountedKeyedService> BuildProfileNoDatabaseForTesting(
      content::BrowserContext* profile);

 private:
  friend base::NoDestructor<ShortcutsBackendFactory>;

  ShortcutsBackendFactory();
  ~ShortcutsBackendFactory() override;

  // BrowserContextKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsNULLWhileTesting() const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;

  static scoped_refptr<ShortcutsBackend> CreateShortcutsBackend(
      Profile* profile,
      bool suppress_db);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_BACKEND_FACTORY_H_
