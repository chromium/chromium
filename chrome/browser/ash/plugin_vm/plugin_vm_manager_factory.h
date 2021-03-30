// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace plugin_vm {

class PluginVmManager;

class PluginVmManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PluginVmManager* GetForProfile(Profile* profile);

  static PluginVmManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<PluginVmManagerFactory>;

  PluginVmManagerFactory();

  ~PluginVmManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_FACTORY_H_
