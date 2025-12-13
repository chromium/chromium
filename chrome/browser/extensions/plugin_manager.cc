// Copyright 2013 The Chromium Authors
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
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "net/base/mime_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

using content::PluginService;

namespace extensions {

PluginManager::PluginManager(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
}

PluginManager::~PluginManager() = default;

static base::LazyInstance<BrowserContextKeyedAPIFactory<PluginManager>>::
    DestructorAtExit g_plugin_manager_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<PluginManager>*
PluginManager::GetFactoryInstance() {
  return g_plugin_manager_factory.Pointer();
}

void PluginManager::OnExtensionLoaded(content::BrowserContext* browser_context,
                                      const Extension* extension) {
  const MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (handler && handler->HasPlugin()) {
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

    PluginService::GetInstance()->RegisterInternalPlugin(info);
    PluginService::GetInstance()->GetPlugins();
    PluginService::GetInstance()->PurgePluginListCache(profile_);
  }
}

void PluginManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  const MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (handler && handler->HasPlugin()) {
    base::FilePath path = handler->GetPluginPath();
    PluginService::GetInstance()->UnregisterInternalPlugin(path);
    PluginService::GetInstance()->GetPlugins();
    PluginService::GetInstance()->PurgePluginListCache(profile_);
  }
}

}  // namespace extensions
