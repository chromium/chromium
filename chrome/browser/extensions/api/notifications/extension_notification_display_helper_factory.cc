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
  return base::Singleton<ExtensionNotificationDisplayHelperFactory>::get();
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
          ProfileSelections::BuildForRegularAndIncognito()) {}

ExtensionNotificationDisplayHelperFactory::
    ~ExtensionNotificationDisplayHelperFactory() {}

KeyedService*
ExtensionNotificationDisplayHelperFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ExtensionNotificationDisplayHelper(profile);
}

}  // namespace extensions
