// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_utils.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/mime_handler/mime_handler_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace {

bool IsExtensionAllowedInProfile(Profile* profile,
                                 const extensions::ExtensionId& extension_id) {
  if (extension_id == extension_misc::kPdfExtensionId &&
      profile->GetPrefs()->GetBoolean(prefs::kPluginsAlwaysOpenPdfExternally)) {
    return false;
  }
  if (profile->IsOffTheRecord() &&
      !extensions::util::IsIncognitoEnabled(extension_id, profile)) {
    return false;
  }
  return true;
}

bool IsExtensionAllowedForMimeType(Profile* profile,
                                   const extensions::ExtensionId& extension_id,
                                   const std::string& mime_type,
                                   bool embedded) {
  if (!IsExtensionAllowedInProfile(profile, extension_id)) {
    return false;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          extension_id);
  if (!extension) {
    return false;
  }

  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  if (!handler) {
    return false;
  }

  if (!embedded) {
    return true;
  }

  return handler->IsPluginExtension() || handler->CanEmbedMimeType(mime_type);
}

}  // namespace
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// static
void PluginUtils::GetPluginContentSetting(
    const HostContentSettingsMap* host_content_settings_map,
    const content::WebPluginInfo& plugin,
    const url::Origin& main_frame_origin,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    bool* is_managed) {
  GURL main_frame_url = main_frame_origin.GetURL();
  content_settings::SettingInfo info;
  *setting = host_content_settings_map->GetContentSetting(
      main_frame_url, main_frame_url, ContentSettingsType::JAVASCRIPT, &info);
  *is_managed = info.source == content_settings::SettingSource::kPolicy;
}

// static
std::string PluginUtils::GetExtensionIdForMimeType(
    content::BrowserContext* browser_context,
    const std::string& mime_type,
    bool embedded) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(profile)) {
    return std::string();
  }
  auto* registry = extensions::MimeHandlerRegistry::Get(browser_context);
  CHECK(registry);
  for (const extensions::ExtensionId& extension_id :
       registry->GetHandlersForMimeType(mime_type)) {
    if (IsExtensionAllowedForMimeType(profile, extension_id, mime_type,
                                      embedded)) {
      return extension_id;
    }
  }
  return std::string();
#else
  return std::string();
#endif
}

base::flat_map<std::string, std::string>
PluginUtils::GetMimeTypeToExtensionIdMap(
    content::BrowserContext* browser_context) {
  base::flat_map<std::string, std::string> mime_type_to_extension_id_map;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(profile)) {
    return mime_type_to_extension_id_map;
  }
  extensions::MimeHandlerRegistry* registry =
      extensions::MimeHandlerRegistry::Get(browser_context);
  CHECK(registry);
  for (const auto& [mime_type, handlers] : registry->GetHandlersByMimeType()) {
    for (const extensions::ExtensionId& extension_id : handlers) {
      if (IsExtensionAllowedInProfile(profile, extension_id)) {
        mime_type_to_extension_id_map[mime_type] = extension_id;
        break;
      }
    }
  }
#endif
  return mime_type_to_extension_id_map;
}
