// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

bool ArcAppListPrefsFactory::is_sync_test_ = false;

// static
ArcAppListPrefs* ArcAppListPrefsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcAppListPrefs*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ArcAppListPrefsFactory* ArcAppListPrefsFactory::GetInstance() {
  static base::NoDestructor<ArcAppListPrefsFactory> instance;
  return instance.get();
}

// static
void ArcAppListPrefsFactory::SetFactoryForSyncTest() {
  is_sync_test_ = true;
}

// static
bool ArcAppListPrefsFactory::IsFactorySetForSyncTest() {
  return is_sync_test_;
}

void ArcAppListPrefsFactory::RecreateServiceInstanceForTesting(
    content::BrowserContext* context) {
  Disassociate(context);
  BuildServiceInstanceFor(context);
}

ArcAppListPrefsFactory::ArcAppListPrefsFactory()
    : ProfileKeyedServiceFactory(
          "ArcAppListPrefs",
          // This matches the logic in ExtensionSyncServiceFactory, which uses
          // the original browser context.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

ArcAppListPrefsFactory::~ArcAppListPrefsFactory() = default;

KeyedService* ArcAppListPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  // Profiles are always treated as allowed to ARC in sync integration test.
  if (is_sync_test_) {
    sync_test_app_connection_holders_[context] = std::make_unique<
        arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>>();
    return ArcAppListPrefs::Create(
        profile, sync_test_app_connection_holders_[context].get());
  }

  if (!arc::IsArcAllowedForProfile(profile))
    return nullptr;

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return nullptr;  // ARC is not supported

  return ArcAppListPrefs::Create(profile);
}
