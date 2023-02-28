// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_export_helper.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/net/service_providers_win.h"
#endif

namespace chrome_browser_net {

base::Value::Dict GetPrerenderInfo(Profile* profile) {
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile);
  if (no_state_prefetch_manager) {
    return no_state_prefetch_manager->CopyAsDict();
  } else {
    base::Value::Dict dict;
    dict.Set("enabled", false);
    dict.Set("omnibox_enabled", false);
    return dict;
  }
}

base::Value::List GetExtensionInfo(Profile* profile) {
  base::Value::List extension_list;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  if (extension_system) {
    extensions::ExtensionService* extension_service =
        extension_system->extension_service();
    if (extension_service) {
      const extensions::ExtensionSet extensions =
          extensions::ExtensionRegistry::Get(profile)
              ->GenerateInstalledExtensionsSet();
      for (const auto& extension : extensions) {
        base::Value::Dict extension_info;
        bool enabled = extension_service->IsExtensionEnabled(extension->id());
        extensions::GetExtensionBasicInfo(extension.get(), enabled,
                                          &extension_info);
        extension_list.Append(std::move(extension_info));
      }
    }
  }
#endif
  return extension_list;
}

#if BUILDFLAG(IS_WIN)
base::Value::Dict GetWindowsServiceProviders() {
  base::Value::Dict service_providers;

  WinsockLayeredServiceProviderList layered_providers;
  GetWinsockLayeredServiceProviders(&layered_providers);
  base::Value::List layered_provider_list;
  for (const auto& provider : layered_providers) {
    base::Value::Dict service_dict;
    service_dict.Set("name", base::AsString16(provider.name));
    service_dict.Set("version", provider.version);
    service_dict.Set("chain_length", provider.chain_length);
    service_dict.Set("socket_type", provider.socket_type);
    service_dict.Set("socket_protocol", provider.socket_protocol);
    service_dict.Set("path", base::WideToUTF8(provider.path));

    layered_provider_list.Append(std::move(service_dict));
  }
  service_providers.Set("service_providers", std::move(layered_provider_list));

  WinsockNamespaceProviderList namespace_providers;
  GetWinsockNamespaceProviders(&namespace_providers);
  base::Value::List namespace_list;
  for (const auto& provider : namespace_providers) {
    base::Value::Dict namespace_dict;
    namespace_dict.Set("name", base::AsString16(provider.name));
    namespace_dict.Set("active", provider.active);
    namespace_dict.Set("version", provider.version);
    namespace_dict.Set("type", provider.type);

    namespace_list.Append(std::move(namespace_dict));
  }
  service_providers.Set("namespace_providers", std::move(namespace_list));

  return service_providers;
}
#endif

}  // namespace chrome_browser_net
