// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"

namespace crostini {

// static
CrostiniManager* CrostiniManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<CrostiniManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniManagerFactory* CrostiniManagerFactory::GetInstance() {
  static base::NoDestructor<CrostiniManagerFactory> factory;
  return factory.get();
}

CrostiniManagerFactory::CrostiniManagerFactory()
    : ProfileKeyedServiceFactory(
          "CrostiniManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

CrostiniManagerFactory::~CrostiniManagerFactory() = default;

std::unique_ptr<KeyedService>
CrostiniManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Exceptionally allow g_browser_process usage here since this class is in
  // base::NoDestructor.
  const auto* component_update_service = g_browser_process->component_updater();
  auto shared_url_loader_factory =
      g_browser_process->shared_url_loader_factory();
  auto component_manager_ash =
      g_browser_process->platform_part()->component_manager_ash();
  auto* scheduler_configuration_manager =
      g_browser_process->platform_part()->scheduler_configuration_manager();

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<CrostiniManager>(
      component_update_service, std::move(shared_url_loader_factory),
      component_manager_ash, scheduler_configuration_manager, profile);
}

}  // namespace crostini
