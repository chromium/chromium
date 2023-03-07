// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/help_app/help_app_ui_delegate.h"

#include <string>

#include "ash/webui/help_app_ui/url_constants.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "url/gurl.h"

namespace ash {

namespace {
void DeviceInfoCallback(
    ash::help_app::mojom::PageHandler::GetDeviceInfoCallback callback,
    apps::DeviceInfo device_info) {
  std::move(callback).Run(help_app::mojom::DeviceInfo::New(
      /*board=*/device_info.board,
      /*model=*/device_info.model,
      /*user_type=*/device_info.user_type));
}
}  // namespace

ChromeHelpAppUIDelegate::ChromeHelpAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui),
      device_info_manager_(std::make_unique<apps::DeviceInfoManager>(
          Profile::FromWebUI(web_ui))) {}

ChromeHelpAppUIDelegate::~ChromeHelpAppUIDelegate() = default;

absl::optional<std::string> ChromeHelpAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  constexpr char kHelpAppFeedbackCategoryTag[] = "FromHelpApp";
  // We don't change the default description, or add extra diagnostics so those
  // are empty strings.
  chrome::ShowFeedbackPage(GURL(kChromeUIHelpAppURL), profile,
                           chrome::kFeedbackSourceHelpApp,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kHelpAppFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);
  return absl::nullopt;
}

void ChromeHelpAppUIDelegate::ShowParentalControls() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  // The "People" section of OS Settings contains parental controls.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kPeopleSectionPath);
}

PrefService* ChromeHelpAppUIDelegate::GetLocalState() {
  return g_browser_process->local_state();
}

void ChromeHelpAppUIDelegate::MaybeShowDiscoverNotification() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  UserSessionManager::GetInstance()->MaybeShowHelpAppDiscoverNotification(
      profile);
}

void ChromeHelpAppUIDelegate::MaybeShowReleaseNotesNotification() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  UserSessionManager::GetInstance()->MaybeShowHelpAppReleaseNotesNotification(
      profile);
}

void ChromeHelpAppUIDelegate::GetDeviceInfo(
    ash::help_app::mojom::PageHandler::GetDeviceInfoCallback callback) {
  device_info_manager_->GetDeviceInfo(
      base::BindOnce(&DeviceInfoCallback, std::move(callback)));
}

}  // namespace ash
