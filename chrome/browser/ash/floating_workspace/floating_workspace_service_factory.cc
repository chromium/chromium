// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"

namespace ash {

// static
FloatingWorkspaceServiceFactory*
FloatingWorkspaceServiceFactory::GetInstance() {
  static base::NoDestructor<FloatingWorkspaceServiceFactory> instance;
  return instance.get();
}

// static
FloatingWorkspaceService* FloatingWorkspaceServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<FloatingWorkspaceService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

FloatingWorkspaceServiceFactory::FloatingWorkspaceServiceFactory()
    : ProfileKeyedServiceFactory(
          "FloatingWorkspaceServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DeskSyncServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

FloatingWorkspaceServiceFactory::~FloatingWorkspaceServiceFactory() = default;

KeyedService* FloatingWorkspaceServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  FloatingWorkspaceService* service =
      new FloatingWorkspaceService(Profile::FromBrowserContext(context));
  service->Init();
  return service;
}

}  // namespace ash
