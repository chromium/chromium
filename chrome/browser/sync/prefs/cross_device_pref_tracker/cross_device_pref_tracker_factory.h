// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_FACTORY_H_
#define CHROME_BROWSER_SYNC_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"

class Profile;

// Singleton factory that creates and manages one CrossDevicePrefTracker
// instance per Profile. The CrossDevicePrefTracker is responsible for
// observing and sharing non-syncing preference values across a user's
// devices, as described in go/cross-device-pref-tracker.
class CrossDevicePrefTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static CrossDevicePrefTrackerFactory* GetInstance();
  static sync_preferences::CrossDevicePrefTracker* GetForProfile(
      Profile* profile);

  CrossDevicePrefTrackerFactory(const CrossDevicePrefTrackerFactory&) = delete;
  CrossDevicePrefTrackerFactory& operator=(
      const CrossDevicePrefTrackerFactory&) = delete;

 private:
  friend class base::NoDestructor<CrossDevicePrefTrackerFactory>;

  CrossDevicePrefTrackerFactory();
  ~CrossDevicePrefTrackerFactory() override;

  // BrowserContextKeyedServiceFactory override:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_FACTORY_H_
