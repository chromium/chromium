// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eche_app/eche_app_manager_factory.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/eche_app_ui/eche_app_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace eche_app {

// static
EcheAppManager* EcheAppManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<EcheAppManager*>(
      EcheAppManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
EcheAppManagerFactory* EcheAppManagerFactory::GetInstance() {
  return base::Singleton<EcheAppManagerFactory>::get();
}

EcheAppManagerFactory::EcheAppManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "EcheAppManager",
          BrowserContextDependencyManager::GetInstance()) {}

EcheAppManagerFactory::~EcheAppManagerFactory() = default;

KeyedService* EcheAppManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* /*context*/) const {
  if (!features::IsEcheSWAEnabled())
    return nullptr;

  return new EcheAppManager();
}

}  // namespace eche_app
}  // namespace chromeos
