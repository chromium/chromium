// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_WEBSITE_PREFERENCE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_WEBSITE_PREFERENCE_BRIDGE_H_

#include <string>
#include <vector>

#include "components/content_settings/core/common/content_settings.h"

class WebsitePreferenceBridge {
 public:
  // Populate the list of corresponding Android permissions associated with the
  // ContentSettingsType specified.
  static void GetAndroidPermissionsForContentSetting(
      ContentSettingsType content_type,
      std::vector<std::string>* out);
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_WEBSITE_PREFERENCE_BRIDGE_H_
