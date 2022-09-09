// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_UTILS_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_UTILS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/content_settings/core/common/content_settings.h"

class GURL;
class HostContentSettingsMap;

namespace content {
class BrowserContext;
struct WebPluginInfo;
}

namespace url {
class Origin;
}

class PluginUtils {
 public:
  PluginUtils() = delete;
  PluginUtils(const PluginUtils&) = delete;
  PluginUtils& operator=(const PluginUtils&) = delete;

  // |is_default| and |is_managed| may be nullptr. In that case, they aren't
  // set.
  static void GetPluginContentSetting(
      const HostContentSettingsMap* host_content_settings_map,
      const content::WebPluginInfo& plugin,
      const url::Origin& main_frame_origin,
      const GURL& plugin_url,
      const std::string& resource,
      ContentSetting* setting,
      bool* is_default,
      bool* is_managed);

  // If there's an extension that is allowed to handle |mime_type|, returns its
  // ID. Otherwise returns an empty string.
  static std::string GetExtensionIdForMimeType(
      content::BrowserContext* browser_context,
      const std::string& mime_type);

  // Returns a map populated with MIME types that are handled by an extension as
  // keys and the corresponding extensions Ids as values.
  static base::flat_map<std::string, std::string> GetMimeTypeToExtensionIdMap(
      content::BrowserContext* browser_context);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_UTILS_H_
