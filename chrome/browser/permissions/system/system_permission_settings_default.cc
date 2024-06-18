// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/buildflag.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

static_assert(!BUILDFLAG(IS_CHROMEOS_ASH));
static_assert(!BUILDFLAG(IS_MAC));

class SystemPermissionSettingsImpl : public SystemPermissionSettings {
  bool CanPrompt(ContentSettingsType type) const override { return false; }
  bool IsDeniedImpl(ContentSettingsType type) const override { return false; }
  bool IsAllowedImpl(ContentSettingsType type) const override { return true; }

  void OpenSystemSettings(content::WebContents*,
                          ContentSettingsType type) const override {
    // no-op
    NOTREACHED();
  }

  void Request(ContentSettingsType type,
               SystemPermissionResponseCallback callback) override {
    std::move(callback).Run();
    NOTREACHED();
  }
};

std::unique_ptr<SystemPermissionSettings>
SystemPermissionSettings::CreateImpl() {
  return std::make_unique<SystemPermissionSettingsImpl>();
}
