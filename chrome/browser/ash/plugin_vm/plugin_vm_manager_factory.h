// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace plugin_vm {

class PluginVmManager;

class PluginVmManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static PluginVmManager* GetForProfile(Profile* profile);

  static PluginVmManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<PluginVmManagerFactory>;

  PluginVmManagerFactory();

  ~PluginVmManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_FACTORY_H_
