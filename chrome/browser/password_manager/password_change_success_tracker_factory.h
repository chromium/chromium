// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace password_manager {
class PasswordChangeSuccessTracker;
}

namespace content {
class BrowserContext;
}

// Creates instances of |PasswordChangeSuccessTracker| per |BrowserContext|.
class PasswordChangeSuccessTrackerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  PasswordChangeSuccessTrackerFactory();
  ~PasswordChangeSuccessTrackerFactory() override;

  static PasswordChangeSuccessTrackerFactory* GetInstance();
  static password_manager::PasswordChangeSuccessTracker* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_
