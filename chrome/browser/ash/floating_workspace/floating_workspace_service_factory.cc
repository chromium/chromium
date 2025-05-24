// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
FloatingWorkspaceServiceFactory*
FloatingWorkspaceServiceFactory::GetInstance() {
  static base::NoDestructor<FloatingWorkspaceServiceFactory> instance;
  return instance.get();
}

// static
FloatingWorkspaceService* FloatingWorkspaceServiceFactory::GetForProfile(
    content::BrowserContext* browser_context) {
  return static_cast<FloatingWorkspaceService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

FloatingWorkspaceServiceFactory::FloatingWorkspaceServiceFactory()
    : ProfileKeyedServiceFactory(
          "FloatingWorkspaceServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(DeskSyncServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  if (ash::features::IsFloatingSsoAllowed()) {
    DependsOn(ash::floating_sso::FloatingSsoServiceFactory::GetInstance());
  }
}

FloatingWorkspaceServiceFactory::~FloatingWorkspaceServiceFactory() = default;

std::unique_ptr<KeyedService>
FloatingWorkspaceServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!user_manager::UserManager::Get()->IsPrimaryUser(
          BrowserContextHelper::Get()->GetUserByBrowserContext(context))) {
    // Floating Workspace is not supported for non-primary browser profiles.
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  floating_workspace_util::FloatingWorkspaceVersion version =
      floating_workspace_util::FloatingWorkspaceVersion::kNoVersionEnabled;
  if (floating_workspace_util::IsFloatingWorkspaceV1Enabled()) {
    version = floating_workspace_util::FloatingWorkspaceVersion::
        kFloatingWorkspaceV1Enabled;
  } else if (floating_workspace_util::IsFloatingWorkspaceV2Enabled()) {
    version = floating_workspace_util::FloatingWorkspaceVersion::
        kFloatingWorkspaceV2Enabled;
  } else if (floating_workspace_util::IsFloatingSsoEnabled(profile)) {
    // When Floating Workspace feature is disabled, but Floating SSO is enabled,
    // we still want to create FloatingWorkspaceService for auto-sign-out
    // functionality.
    // TODO(crbug.com/419508619): improve naming to avoid confusion of having
    // FloatingWorkspaceService running when Floating Workspace is disabled as a
    // feature.
    version =
        floating_workspace_util::FloatingWorkspaceVersion::kAutoSignoutOnly;
  }
  std::unique_ptr<FloatingWorkspaceService> service =
      std::make_unique<FloatingWorkspaceService>(profile, version);
  service->Init(SyncServiceFactory::GetForProfile(profile),
                DeskSyncServiceFactory::GetForProfile(profile),
                DeviceInfoSyncServiceFactory::GetForProfile(profile));
  return service;
}

}  // namespace ash
