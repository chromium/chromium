// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/plugin_manager.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "net/base/mime_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#endif

using content::PluginService;

namespace extensions {

PluginManager::PluginManager(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
}

PluginManager::~PluginManager() {
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<PluginManager>>::
    DestructorAtExit g_plugin_manager_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<PluginManager>*
PluginManager::GetFactoryInstance() {
  return g_plugin_manager_factory.Pointer();
}

void PluginManager::OnExtensionLoaded(content::BrowserContext* browser_context,
                                      const Extension* extension) {
  bool plugins_or_nacl_changed = false;
#if BUILDFLAG(ENABLE_NACL)
  const NaClModuleInfo::List* nacl_modules =
      NaClModuleInfo::GetNaClModules(extension);
  if (nacl_modules) {
    plugins_or_nacl_changed = true;
    for (auto module = nacl_modules->begin(); module != nacl_modules->end();
         ++module) {
      RegisterNaClModule(*module);
    }
    UpdatePluginListWithNaClModules();
  }
#endif

  const MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (handler && handler->HasPlugin()) {
    plugins_or_nacl_changed = true;

    content::WebPluginInfo info;
    info.type = content::WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN;
    info.name = base::UTF8ToUTF16(extension->name());
    info.path = handler->GetPluginPath();
    info.background_color = handler->GetBackgroundColor();

    for (auto mime_type = handler->mime_type_set().begin();
         mime_type != handler->mime_type_set().end(); ++mime_type) {
      content::WebPluginMimeType mime_type_info;
      mime_type_info.mime_type = *mime_type;
      base::FilePath::StringType file_extension;
      if (net::GetPreferredExtensionForMimeType(*mime_type, &file_extension)) {
        mime_type_info.file_extensions.push_back(
            base::FilePath(file_extension).AsUTF8Unsafe());
      }
      info.mime_types.push_back(mime_type_info);
    }

    PluginService::GetInstance()->RefreshPlugins();
    PluginService::GetInstance()->RegisterInternalPlugin(info, true);
  }

  if (plugins_or_nacl_changed)
    PluginService::GetInstance()->PurgePluginListCache(profile_, false);
}

void PluginManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  bool plugins_or_nacl_changed = false;
#if BUILDFLAG(ENABLE_NACL)
  const NaClModuleInfo::List* nacl_modules =
      NaClModuleInfo::GetNaClModules(extension);
  if (nacl_modules) {
    plugins_or_nacl_changed = true;
    for (auto module = nacl_modules->begin(); module != nacl_modules->end();
         ++module) {
      UnregisterNaClModule(*module);
    }
    UpdatePluginListWithNaClModules();
  }
#endif

  const MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (handler && handler->HasPlugin()) {
    plugins_or_nacl_changed = true;
    base::FilePath path = handler->GetPluginPath();
    PluginService::GetInstance()->UnregisterInternalPlugin(path);
    PluginService::GetInstance()->RefreshPlugins();
  }

  if (plugins_or_nacl_changed)
    PluginService::GetInstance()->PurgePluginListCache(profile_, false);
}

#if BUILDFLAG(ENABLE_NACL)

void PluginManager::RegisterNaClModule(const NaClModuleInfo& info) {
  nacl_module_list_.push_front(info);
}

void PluginManager::UnregisterNaClModule(const NaClModuleInfo& info) {
  auto iter = FindNaClModule(info.url);
  if (iter != nacl_module_list_.end())
    nacl_module_list_.erase(iter);
}

void PluginManager::UpdatePluginListWithNaClModules() {
  // An extension has been added which has a nacl_module component, which means
  // there is a MIME type that module wants to handle, so we need to add that
  // MIME type to plugins which handle NaCl modules in order to allow the
  // individual modules to handle these types.
  static const base::NoDestructor<base::FilePath> path(
      ChromeContentClient::kNaClPluginFileName);
  const content::PepperPluginInfo* pepper_info =
      PluginService::GetInstance()->GetRegisteredPpapiPluginInfo(*path);
  if (!pepper_info)
    return;

  std::vector<content::WebPluginMimeType>::const_iterator mime_iter;
  // Check each MIME type the plugins handle for the NaCl MIME type.
  for (mime_iter = pepper_info->mime_types.begin();
       mime_iter != pepper_info->mime_types.end(); ++mime_iter) {
    if (mime_iter->mime_type == nacl::kNaClPluginMimeType) {
      // This plugin handles "application/x-nacl".

      PluginService::GetInstance()->UnregisterInternalPlugin(pepper_info->path);

      content::WebPluginInfo info = pepper_info->ToWebPluginInfo();

      for (NaClModuleInfo::List::const_iterator iter =
               nacl_module_list_.begin();
           iter != nacl_module_list_.end(); ++iter) {
        // Add the MIME type specified in the extension to this NaCl plugin,
        // With an extra "nacl" argument to specify the location of the NaCl
        // manifest file.
        content::WebPluginMimeType mime_type_info;
        mime_type_info.mime_type = iter->mime_type;
        mime_type_info.additional_params.emplace_back(
            base::UTF8ToUTF16("nacl"), base::UTF8ToUTF16(iter->url.spec()));
        info.mime_types.emplace_back(std::move(mime_type_info));
      }

      PluginService::GetInstance()->RefreshPlugins();
      PluginService::GetInstance()->RegisterInternalPlugin(info, true);
      // This plugin has been modified, no need to check the rest of its
      // types, but continue checking other plugins.
      break;
    }
  }
}

NaClModuleInfo::List::iterator PluginManager::FindNaClModule(const GURL& url) {
  for (auto iter = nacl_module_list_.begin(); iter != nacl_module_list_.end();
       ++iter) {
    if (iter->url == url)
      return iter;
  }
  return nacl_module_list_.end();
}

#endif  // BUILDFLAG(ENABLE_NACL)

}  // namespace extensions
