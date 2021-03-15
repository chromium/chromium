// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_INSTALLER_FACTORY_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_INSTALLER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace plugin_vm {

class PluginVmInstaller;

class PluginVmInstallerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PluginVmInstaller* GetForProfile(Profile* profile);
  static PluginVmInstallerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PluginVmInstallerFactory>;

  PluginVmInstallerFactory();
  ~PluginVmInstallerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PluginVmInstallerFactory);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_INSTALLER_FACTORY_H_
