// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_permission_context_factory.h"

#include "base/check_deref.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/smart_card_permission_context.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker_factory.h"

// static
SmartCardPermissionContextFactory*
SmartCardPermissionContextFactory::GetInstance() {
  static base::NoDestructor<SmartCardPermissionContextFactory> factory;
  return factory.get();
}

// static
SmartCardPermissionContext& SmartCardPermissionContextFactory::GetForProfile(
    Profile& profile) {
  return CHECK_DEREF(static_cast<SmartCardPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(&profile, true)));
}

SmartCardPermissionContextFactory::SmartCardPermissionContextFactory()
    : ProfileKeyedServiceFactory(
          "SmartCardPermissionContext",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(OneTimePermissionsTrackerFactory::GetInstance());
  DependsOn(SmartCardReaderTrackerFactory::GetInstance());
}

SmartCardPermissionContextFactory::~SmartCardPermissionContextFactory() =
    default;

std::unique_ptr<KeyedService>
SmartCardPermissionContextFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SmartCardPermissionContext>(
      Profile::FromBrowserContext(context));
}
