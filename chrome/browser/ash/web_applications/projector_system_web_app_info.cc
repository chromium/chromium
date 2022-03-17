// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/projector_system_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/grit/ash_projector_app_trusted_resources.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"

namespace {

SkColor GetBgColor(bool use_dark_mode) {
  return cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor, use_dark_mode,
      base::FeatureList::IsEnabled(
          ash::features::kSemanticColorsDebugOverride));
}

}  // namespace

ProjectorSystemWebAppDelegate::ProjectorSystemWebAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::PROJECTOR,
                                    "Projector",
                                    GURL(ash::kChromeUITrustedProjectorAppUrl),
                                    profile) {}

ProjectorSystemWebAppDelegate::~ProjectorSystemWebAppDelegate() = default;

std::unique_ptr<WebAppInstallInfo>
ProjectorSystemWebAppDelegate::GetWebAppInfo() const {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUITrustedProjectorAppUrl);
  info->scope = GURL(ash::kChromeUITrustedProjectorAppUrl);

  info->title = l10n_util::GetStringUTF16(IDS_PROJECTOR_APP_NAME);

  // TODO(b/195127670): Add 48, 128, and 192 size icons through
  // CreateIconInfoForSystemWebApp().

  info->theme_color = GetBgColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color = GetBgColor(/*use_dark_mode=*/true);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  // TODO(b/195127670): Add info.url_handlers for https://projector.apps.chrome
  // domain. Requires web-app-origin-association file at the new domain to prove
  // cross-ownership. See
  // https://web.dev/pwa-url-handler/#the-web-app-origin-association-file.

  return info;
}

bool ProjectorSystemWebAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

gfx::Size ProjectorSystemWebAppDelegate::GetMinimumWindowSize() const {
  // The minimum width matches the minimum width of the Projector viewer left
  // panel defined in the web component.
  return {222, 550};
}

bool ProjectorSystemWebAppDelegate::IsAppEnabled() const {
  if (!IsProjectorAllowedForProfile(profile_))
    return false;

  if (!profile_->GetProfilePolicyConnector()->IsManaged() ||
      profile_->IsChild()) {
    return ash::features::IsProjectorAllUserEnabled();
  }

  // Check feature availability and admin policy for managed users.
  return ash::features::IsProjectorEnabled() &&
         profile_->GetPrefs()->GetBoolean(ash::prefs::kProjectorAllowByPolicy);
}
