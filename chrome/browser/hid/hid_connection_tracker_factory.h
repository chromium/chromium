// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_FACTORY_H_
#define CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class HidConnectionTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static HidConnectionTracker* GetForProfile(Profile* profile, bool create);
  static HidConnectionTrackerFactory* GetInstance();

  HidConnectionTrackerFactory(const HidConnectionTrackerFactory&) = delete;
  HidConnectionTrackerFactory& operator=(const HidConnectionTrackerFactory&) =
      delete;

 private:
  friend base::NoDestructor<HidConnectionTrackerFactory>;

  HidConnectionTrackerFactory();
  ~HidConnectionTrackerFactory() override;

  // BrowserContextKeyedBaseFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_FACTORY_H_
