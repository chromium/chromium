// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_PROVIDER_CONTENT_SETTINGS_HANDLER_H_
#define ASH_WEBUI_BOCA_UI_PROVIDER_CONTENT_SETTINGS_HANDLER_H_

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "base/memory/raw_ptr.h"

class Profile;

namespace ash::boca {

class ContentSettingsHandler {
 public:
  explicit ContentSettingsHandler(Profile* profile);
  ContentSettingsHandler(const ContentSettingsHandler&) = delete;
  ContentSettingsHandler& operator=(const ContentSettingsHandler&) = delete;
  ~ContentSettingsHandler();

  bool SetContentSettingForOrigin(const std::string& url,
                                  mojom::Permission content_type,
                                  mojom::PermissionSetting setting);

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_PROVIDER_CONTENT_SETTINGS_HANDLER_H_
