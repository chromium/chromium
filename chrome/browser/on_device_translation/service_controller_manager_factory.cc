// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/service_controller_manager_factory.h"

#include "base/memory/singleton.h"
#include "base/types/pass_key.h"
#include "chrome/browser/on_device_translation/service_controller_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace on_device_translation {

ServiceControllerManagerFactory::ServiceControllerManagerFactory()
    : ProfileKeyedServiceFactory(
          "OnDeviceTranslationServiceControllerManager",
          // We support both regular and guest profiles including the off the
          // record profile
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

// static
ServiceControllerManagerFactory*
ServiceControllerManagerFactory::GetInstance() {
  return base::Singleton<ServiceControllerManagerFactory>::get();
}

ServiceControllerManager* ServiceControllerManagerFactory::Get(
    content::BrowserContext* context) {
  return static_cast<ServiceControllerManager*>(
      GetServiceForBrowserContext(context, true));
}

std::unique_ptr<KeyedService>
ServiceControllerManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ServiceControllerManager>(
      base::PassKey<ServiceControllerManagerFactory>());
}

}  // namespace on_device_translation
