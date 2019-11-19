// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace apps {

class LaunchService;

// A factory which associates LaunchService instance with its profile instance.
class LaunchServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LaunchService* GetForProfile(Profile* profile);

  static LaunchServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<LaunchServiceFactory>;

  LaunchServiceFactory();
  ~LaunchServiceFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(LaunchServiceFactory);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LAUNCH_SERVICE_LAUNCH_SERVICE_FACTORY_H_
