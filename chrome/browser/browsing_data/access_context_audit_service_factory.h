// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class AccessContextAuditService;

class AccessContextAuditServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AccessContextAuditServiceFactory* GetInstance();
  static AccessContextAuditService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<AccessContextAuditServiceFactory>;
  AccessContextAuditServiceFactory();
  ~AccessContextAuditServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_FACTORY_H_
