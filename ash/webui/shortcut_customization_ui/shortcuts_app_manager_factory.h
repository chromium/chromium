// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUTS_APP_MANAGER_FACTORY_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUTS_APP_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace ash::shortcut_ui {

class ShortcutsAppManager;

// This class is responsible for the creation of the ShortcutsAppManager.
class ShortcutsAppManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ShortcutsAppManager* GetForBrowserContext(
      content::BrowserContext* context);
  static ShortcutsAppManagerFactory* GetInstance();

  ShortcutsAppManagerFactory(const ShortcutsAppManagerFactory&) = delete;
  ShortcutsAppManagerFactory& operator=(const ShortcutsAppManagerFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<ShortcutsAppManagerFactory>;

  ShortcutsAppManagerFactory();
  ~ShortcutsAppManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash::shortcut_ui

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUTS_APP_MANAGER_FACTORY_H_
