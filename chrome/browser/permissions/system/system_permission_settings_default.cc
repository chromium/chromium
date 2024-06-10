// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

class SystemPermissionSettingsImpl : public SystemPermissionSettings {
  bool IsPermissionDeniedImpl(ContentSettingsType type) const override {
    return false;
  }

  void OpenSystemSettings(content::WebContents*,
                          ContentSettingsType type) const override {
    // no-op
  }
};

std::unique_ptr<SystemPermissionSettings> SystemPermissionSettings::Create() {
  return std::make_unique<SystemPermissionSettingsImpl>();
}
