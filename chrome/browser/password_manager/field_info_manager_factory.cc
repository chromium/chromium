// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/field_info_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/field_info_manager.h"

using password_manager::FieldInfoManager;

// static
FieldInfoManagerFactory* FieldInfoManagerFactory::GetInstance() {
  static base::NoDestructor<FieldInfoManagerFactory> instance;
  return instance.get();
}

// static
FieldInfoManager* FieldInfoManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<FieldInfoManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

FieldInfoManagerFactory::FieldInfoManagerFactory()
    : ProfileKeyedServiceFactory(
          "FieldInfoManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

FieldInfoManagerFactory::~FieldInfoManagerFactory() = default;

// BrowserContextKeyedServiceFactory overrides:
std::unique_ptr<KeyedService>
FieldInfoManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<FieldInfoManager>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
}
