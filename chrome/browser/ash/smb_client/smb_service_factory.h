// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace smb_client {

class SmbService;

class SmbServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns a service instance singleton, after creating it (if necessary).
  static SmbService* Get(content::BrowserContext* context);

  // Returns a service instance for the context if exists. Otherwise, returns
  // NULL.
  static SmbService* FindExisting(content::BrowserContext* context);

  // Gets a singleton instance of the factory.
  static SmbServiceFactory* GetInstance();

  // Disallow copy and assignment.
  SmbServiceFactory(const SmbServiceFactory&) = delete;
  SmbServiceFactory& operator=(const SmbServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SmbServiceFactory>;

  SmbServiceFactory();
  ~SmbServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace smb_client
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_FACTORY_H_
