// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/web_site_settings_uma_util.h"

#include <stddef.h>

#include "base/metrics/histogram_macros.h"

void WebSiteSettingsUmaUtil::LogPermissionChange(ContentSettingsType type,
                                                 ContentSetting setting) {
  size_t num_values;
  int histogram_value = ContentSettingTypeToHistogramValue(type, &num_values);
  UMA_HISTOGRAM_EXACT_LINEAR("WebsiteSettings.Menu.PermissionChanged",
                             histogram_value, num_values);

  if (setting == ContentSetting::CONTENT_SETTING_ALLOW) {
    UMA_HISTOGRAM_EXACT_LINEAR("WebsiteSettings.Menu.PermissionChanged.Allowed",
                               histogram_value, num_values);
  } else if (setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    UMA_HISTOGRAM_EXACT_LINEAR("WebsiteSettings.Menu.PermissionChanged.Blocked",
                               histogram_value, num_values);
  } else if (setting == ContentSetting::CONTENT_SETTING_ASK) {
    UMA_HISTOGRAM_EXACT_LINEAR("WebsiteSettings.Menu.PermissionChanged.Ask",
                               histogram_value, num_values);
  } else if (setting == ContentSetting::CONTENT_SETTING_DEFAULT) {
    UMA_HISTOGRAM_EXACT_LINEAR("WebsiteSettings.Menu.PermissionChanged.Reset",
                               histogram_value, num_values);
  } else if (setting == ContentSetting::CONTENT_SETTING_SESSION_ONLY) {
    DCHECK_EQ(ContentSettingsType::COOKIES, type);
    UMA_HISTOGRAM_EXACT_LINEAR(
        "WebsiteSettings.Menu.PermissionChanged.SessionOnly", histogram_value,
        num_values);
  } else {
    NOTREACHED() << "Requested to log permission change "
                 << static_cast<int32_t>(type) << " to " << setting;
  }
}
