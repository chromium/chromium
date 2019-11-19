// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_STATE_TRACKER_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_STATE_TRACKER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class NotifierStateTracker;
class Profile;

class NotifierStateTrackerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static NotifierStateTracker* GetForProfile(Profile* profile);
  static NotifierStateTrackerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NotifierStateTrackerFactory>;

  NotifierStateTrackerFactory();
  ~NotifierStateTrackerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(NotifierStateTrackerFactory);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_STATE_TRACKER_FACTORY_H_
