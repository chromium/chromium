// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
// TODO(https://crbug.com/1164001): remove and use forward declaration.
#include "chrome/browser/ash/smb_client/smb_service.h"

namespace ash {
namespace smb_client {

class SmbServiceFactory : public BrowserContextKeyedServiceFactory {
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
  friend struct base::DefaultSingletonTraits<SmbServiceFactory>;

  SmbServiceFactory();
  ~SmbServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace smb_client
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
namespace smb_client {
using ::ash::smb_client::SmbServiceFactory;
}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_FACTORY_H_
