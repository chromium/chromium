// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_PROVIDER_CONTENT_SETTINGS_HANDLER_H_
#define ASH_WEBUI_BOCA_UI_PROVIDER_CONTENT_SETTINGS_HANDLER_H_

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "base/memory/raw_ref.h"

class HostContentSettingsMap;

namespace ash::boca {

class ContentSettingsHandler {
 public:
  // `host_content_settings_map` is the one belonging to the user's profile. It
  // must be non-null and must outlive `this`.
  explicit ContentSettingsHandler(
      HostContentSettingsMap* host_content_settings_map);
  ContentSettingsHandler(const ContentSettingsHandler&) = delete;
  ContentSettingsHandler& operator=(const ContentSettingsHandler&) = delete;
  virtual ~ContentSettingsHandler();

  virtual bool SetContentSettingForOrigin(const std::string& url,
                                          mojom::Permission content_type,
                                          mojom::PermissionSetting setting);

 private:
  const raw_ref<HostContentSettingsMap> host_content_settings_map_;
};

}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_PROVIDER_CONTENT_SETTINGS_HANDLER_H_
