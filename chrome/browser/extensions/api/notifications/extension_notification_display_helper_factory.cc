// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper_factory.h"

#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace extensions {

// static
ExtensionNotificationDisplayHelperFactory*
ExtensionNotificationDisplayHelperFactory::GetInstance() {
  static base::NoDestructor<ExtensionNotificationDisplayHelperFactory> instance;
  return instance.get();
}

// static
ExtensionNotificationDisplayHelper*
ExtensionNotificationDisplayHelperFactory::GetForProfile(Profile* profile) {
  return static_cast<ExtensionNotificationDisplayHelper*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

ExtensionNotificationDisplayHelperFactory::
    ExtensionNotificationDisplayHelperFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionNotificationDisplayHelperFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

ExtensionNotificationDisplayHelperFactory::
    ~ExtensionNotificationDisplayHelperFactory() = default;

std::unique_ptr<KeyedService> ExtensionNotificationDisplayHelperFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ExtensionNotificationDisplayHelper>(profile);
}

}  // namespace extensions
