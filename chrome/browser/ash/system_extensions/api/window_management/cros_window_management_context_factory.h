// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace ash {

class CrosWindowManagementContext;

// Class to retrieve the CrosWindowManagementContext associated with
// a profile.
class CrosWindowManagementContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static CrosWindowManagementContextFactory& GetInstance();

  // CrosWindowManagementContext is created automatically for
  // appropriate profiles e.g. the primary profile.
  static CrosWindowManagementContext* GetForProfileIfExists(Profile* profile);

  CrosWindowManagementContextFactory(
      const CrosWindowManagementContextFactory&) = delete;
  CrosWindowManagementContextFactory& operator=(
      const CrosWindowManagementContextFactory&) = delete;

 private:
  friend base::NoDestructor<CrosWindowManagementContextFactory>;

  CrosWindowManagementContextFactory();
  ~CrosWindowManagementContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_FACTORY_H_
