// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_DISPLAY_HELPER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_DISPLAY_HELPER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace extensions {

class ExtensionNotificationDisplayHelper;

class ExtensionNotificationDisplayHelperFactory
    : public ProfileKeyedServiceFactory {
 public:
  ExtensionNotificationDisplayHelperFactory(
      const ExtensionNotificationDisplayHelperFactory&) = delete;
  ExtensionNotificationDisplayHelperFactory& operator=(
      const ExtensionNotificationDisplayHelperFactory&) = delete;

  // Get the singleton instance of the factory.
  static ExtensionNotificationDisplayHelperFactory* GetInstance();

  // Get the display helper for |profile|, creating one if needed.
  static ExtensionNotificationDisplayHelper* GetForProfile(Profile* profile);

 protected:
  // Overridden from BrowserContextKeyedServiceFactory.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<ExtensionNotificationDisplayHelperFactory>;

  ExtensionNotificationDisplayHelperFactory();
  ~ExtensionNotificationDisplayHelperFactory() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_DISPLAY_HELPER_FACTORY_H_
