// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_HELP_APP_MANAGER_FACTORY_H_
#define ASH_WEBUI_HELP_APP_UI_HELP_APP_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace ash {
namespace help_app {

class HelpAppManager;

class HelpAppManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static HelpAppManager* GetForBrowserContext(content::BrowserContext* context);
  static HelpAppManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<HelpAppManagerFactory>;

  HelpAppManagerFactory();
  ~HelpAppManagerFactory() override;

  HelpAppManagerFactory(const HelpAppManagerFactory&) = delete;
  HelpAppManagerFactory& operator=(const HelpAppManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace help_app
}  // namespace ash

#endif  // ASH_WEBUI_HELP_APP_UI_HELP_APP_MANAGER_FACTORY_H_
