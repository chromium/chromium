// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_export_helper.h"

#include "base/values.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/net/service_providers_win.h"
#endif

namespace chrome_browser_net {

std::unique_ptr<base::DictionaryValue> GetPrerenderInfo(Profile* profile) {
  std::unique_ptr<base::DictionaryValue> value;
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile);
  if (prerender_manager) {
    value = prerender_manager->CopyAsValue();
  } else {
    value.reset(new base::DictionaryValue());
    value->SetBoolean("enabled", false);
    value->SetBoolean("omnibox_enabled", false);
  }
  return value;
}

std::unique_ptr<base::ListValue> GetExtensionInfo(Profile* profile) {
  auto extension_list = std::make_unique<base::ListValue>();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  if (extension_system) {
    extensions::ExtensionService* extension_service =
        extension_system->extension_service();
    if (extension_service) {
      std::unique_ptr<const extensions::ExtensionSet> extensions(
          extensions::ExtensionRegistry::Get(profile)
              ->GenerateInstalledExtensionsSet());
      for (const auto& extension : *extensions) {
        std::unique_ptr<base::DictionaryValue> extension_info(
            new base::DictionaryValue());
        bool enabled = extension_service->IsExtensionEnabled(extension->id());
        extensions::GetExtensionBasicInfo(extension.get(), enabled,
                                          extension_info.get());
        extension_list->Append(std::move(extension_info));
      }
    }
  }
#endif
  return extension_list;
}

#if defined(OS_WIN)
std::unique_ptr<base::DictionaryValue> GetWindowsServiceProviders() {
  auto service_providers = std::make_unique<base::DictionaryValue>();

  WinsockLayeredServiceProviderList layered_providers;
  GetWinsockLayeredServiceProviders(&layered_providers);
  auto layered_provider_list = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < layered_providers.size(); ++i) {
    auto service_dict = std::make_unique<base::DictionaryValue>();
    service_dict->SetString("name", layered_providers[i].name);
    service_dict->SetInteger("version", layered_providers[i].version);
    service_dict->SetInteger("chain_length", layered_providers[i].chain_length);
    service_dict->SetInteger("socket_type", layered_providers[i].socket_type);
    service_dict->SetInteger("socket_protocol",
                             layered_providers[i].socket_protocol);
    service_dict->SetString("path", layered_providers[i].path);

    layered_provider_list->Append(std::move(service_dict));
  }
  service_providers->Set("service_providers", std::move(layered_provider_list));

  WinsockNamespaceProviderList namespace_providers;
  GetWinsockNamespaceProviders(&namespace_providers);
  auto namespace_list = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < namespace_providers.size(); ++i) {
    auto namespace_dict = std::make_unique<base::DictionaryValue>();
    namespace_dict->SetString("name", namespace_providers[i].name);
    namespace_dict->SetBoolean("active", namespace_providers[i].active);
    namespace_dict->SetInteger("version", namespace_providers[i].version);
    namespace_dict->SetInteger("type", namespace_providers[i].type);

    namespace_list->Append(std::move(namespace_dict));
  }
  service_providers->Set("namespace_providers", std::move(namespace_list));

  return service_providers;
}
#endif

}  // namespace chrome_browser_net
