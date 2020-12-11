// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_APP_TERMINATION_OBSERVER_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_APP_TERMINATION_OBSERVER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}

namespace chrome_apps {

// A helper class to observe application termination, and notify the
// appropriate apps systems.
class AppTerminationObserver : public content::NotificationObserver,
                               public KeyedService {
 public:
  explicit AppTerminationObserver(content::BrowserContext* browser_context);
  ~AppTerminationObserver() override;

  static BrowserContextKeyedServiceFactory* GetFactoryInstance();

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  content::BrowserContext* browser_context_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppTerminationObserver);
};

}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_APP_TERMINATION_OBSERVER_H_
