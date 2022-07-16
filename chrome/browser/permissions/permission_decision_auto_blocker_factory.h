// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace permissions {
class PermissionDecisionAutoBlocker;
}

class PermissionDecisionAutoBlockerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static permissions::PermissionDecisionAutoBlocker* GetForProfile(
      Profile* profile);
  static PermissionDecisionAutoBlockerFactory* GetInstance();

  PermissionDecisionAutoBlockerFactory(
      const PermissionDecisionAutoBlockerFactory&) = delete;
  PermissionDecisionAutoBlockerFactory& operator=(
      const PermissionDecisionAutoBlockerFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      PermissionDecisionAutoBlockerFactory>;

  PermissionDecisionAutoBlockerFactory();
  ~PermissionDecisionAutoBlockerFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_FACTORY_H_
