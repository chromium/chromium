// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_MANAGEMENT_SERVICE_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

// A keyed service aggregating services for respective RPCs in
// KidsManagementAPI.
class KidsManagementService : public KeyedService {};

// The framework binding for the KidsManagementAPI service.
class KidsManagementServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static KidsManagementService* GetForProfile(Profile* profile);
  static KidsManagementServiceFactory* GetInstance();

  KidsManagementServiceFactory(const KidsManagementServiceFactory&) = delete;
  KidsManagementServiceFactory& operator=(const KidsManagementServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<KidsManagementServiceFactory>;

  KidsManagementServiceFactory();
  ~KidsManagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_MANAGEMENT_SERVICE_H_
