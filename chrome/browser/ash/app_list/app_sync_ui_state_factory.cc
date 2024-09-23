// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_sync_ui_state_factory.h"

#include "chrome/browser/ash/app_list/app_sync_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "extensions/browser/extension_registry_factory.h"

// static
AppSyncUIState* AppSyncUIStateFactory::GetForProfile(Profile* profile) {
  if (!AppSyncUIState::ShouldObserveAppSyncForProfile(profile))
    return nullptr;

  return static_cast<AppSyncUIState*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppSyncUIStateFactory* AppSyncUIStateFactory::GetInstance() {
  static base::NoDestructor<AppSyncUIStateFactory> instance;
  return instance.get();
}

AppSyncUIStateFactory::AppSyncUIStateFactory()
    : ProfileKeyedServiceFactory(
          "AppSyncUIState",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

AppSyncUIStateFactory::~AppSyncUIStateFactory() = default;

std::unique_ptr<KeyedService>
AppSyncUIStateFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(AppSyncUIState::ShouldObserveAppSyncForProfile(profile));
  return std::make_unique<AppSyncUIState>(profile);
}
