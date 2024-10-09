// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace ash {
// static
BocaManagerFactory* BocaManagerFactory::GetInstance() {
  static base::NoDestructor<BocaManagerFactory> instance;
  return instance.get();
}

// static
BocaManager* BocaManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<BocaManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

BocaManagerFactory::BocaManagerFactory()
    : ProfileKeyedServiceFactory(
          "BocaManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // Do not init for ash internal such as login and lock screen.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

BocaManagerFactory::~BocaManagerFactory() = default;

std::unique_ptr<KeyedService>
BocaManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(boca_util::IsEnabled());
  Profile* profile = Profile::FromBrowserContext(context);
  auto service = std::make_unique<BocaManager>(profile);
  return service;
}

}  // namespace ash
