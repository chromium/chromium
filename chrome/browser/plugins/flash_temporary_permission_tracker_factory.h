// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_FLASH_TEMPORARY_PERMISSION_TRACKER_FACTORY_H_
#define CHROME_BROWSER_PLUGINS_FLASH_TEMPORARY_PERMISSION_TRACKER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class FlashTemporaryPermissionTracker;
class Profile;

class FlashTemporaryPermissionTrackerFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  static scoped_refptr<FlashTemporaryPermissionTracker> GetForProfile(
      Profile* profile);
  static FlashTemporaryPermissionTrackerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      FlashTemporaryPermissionTrackerFactory>;

  FlashTemporaryPermissionTrackerFactory();
  ~FlashTemporaryPermissionTrackerFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory methods:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(FlashTemporaryPermissionTrackerFactory);
};

#endif  // CHROME_BROWSER_PLUGINS_FLASH_TEMPORARY_PERMISSION_TRACKER_FACTORY_H_
