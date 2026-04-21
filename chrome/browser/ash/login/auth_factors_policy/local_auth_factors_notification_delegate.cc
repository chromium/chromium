// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth_factors_policy/local_auth_factors_notification_delegate.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "components/user_manager/user.h"

namespace ash {

LocalAuthFactorsNotificationDelegate::LocalAuthFactorsNotificationDelegate(
    Profile* profile)
    : profile_(profile) {}

LocalAuthFactorsNotificationDelegate::~LocalAuthFactorsNotificationDelegate() =
    default;

void LocalAuthFactorsNotificationDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (button_index.has_value()) {
    auto* user =
        ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile_);
    if (user) {
      ash::SettingsAppManager::Get()->Open(
          *user,
          {.sub_page =
               chromeos::settings::mojom::kSecurityAndSignInSubpagePathV2});
    }
  }
}

}  // namespace ash
