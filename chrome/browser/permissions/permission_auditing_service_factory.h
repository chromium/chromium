// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_AUDITING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_AUDITING_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace permissions {
class PermissionAuditingService;
}

class PermissionAuditingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PermissionAuditingServiceFactory* GetInstance();

  // Creates a permission auditing service for the given `profile`. Will return
  // nullptr in case if |profile| is off-the-record or if `kPermissionAuditing`
  // feature is disabled.
  static permissions::PermissionAuditingService* GetForProfile(
      Profile* profile);

  PermissionAuditingServiceFactory(const PermissionAuditingServiceFactory&) =
      delete;
  PermissionAuditingServiceFactory& operator=(
      const PermissionAuditingServiceFactory&) = delete;

  PermissionAuditingServiceFactory(PermissionAuditingServiceFactory&&) = delete;
  PermissionAuditingServiceFactory& operator=(
      PermissionAuditingServiceFactory&&) = delete;

 private:
  friend base::NoDestructor<PermissionAuditingServiceFactory>;

  PermissionAuditingServiceFactory();
  ~PermissionAuditingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_AUDITING_SERVICE_FACTORY_H_
