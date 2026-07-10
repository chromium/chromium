// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CROSS_DEVICE_THEME_TRACKER_FACTORY_H_
#define CHROME_BROWSER_SYNC_CROSS_DEVICE_THEME_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/themes/cross_device/cross_device_theme_tracker.h"

class Profile;

class CrossDeviceThemeTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  using LocalThemeSpecifics = themes::LocalThemeSpecifics;

  static themes::CrossDeviceThemeTracker<LocalThemeSpecifics>* GetForProfile(
      Profile* profile);
  static CrossDeviceThemeTrackerFactory* GetInstance();

 private:
  friend class base::NoDestructor<CrossDeviceThemeTrackerFactory>;

  CrossDeviceThemeTrackerFactory();
  ~CrossDeviceThemeTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_CROSS_DEVICE_THEME_TRACKER_FACTORY_H_
