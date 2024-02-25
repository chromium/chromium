// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class SmartCardPermissionContext;

class SmartCardPermissionContextFactory : public ProfileKeyedServiceFactory {
 public:
  static SmartCardPermissionContext& GetForProfile(Profile& profile);
  static SmartCardPermissionContextFactory* GetInstance();

  SmartCardPermissionContextFactory(const SmartCardPermissionContextFactory&) =
      delete;
  SmartCardPermissionContextFactory& operator=(
      const SmartCardPermissionContextFactory&) = delete;

 private:
  friend base::NoDestructor<SmartCardPermissionContextFactory>;

  SmartCardPermissionContextFactory();
  ~SmartCardPermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_CONTEXT_FACTORY_H_
