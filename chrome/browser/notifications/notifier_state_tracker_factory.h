// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_STATE_TRACKER_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_STATE_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class NotifierStateTracker;
class Profile;

class NotifierStateTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static NotifierStateTracker* GetForProfile(Profile* profile);
  static NotifierStateTrackerFactory* GetInstance();

 private:
  friend base::NoDestructor<NotifierStateTrackerFactory>;

  NotifierStateTrackerFactory();
  NotifierStateTrackerFactory(const NotifierStateTrackerFactory&) = delete;
  NotifierStateTrackerFactory& operator=(const NotifierStateTrackerFactory&) =
      delete;
  ~NotifierStateTrackerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_STATE_TRACKER_FACTORY_H_
