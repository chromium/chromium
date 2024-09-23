// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/install_web_app_action_performer.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace {

inline constexpr char kLaunchInStandaloneWindow[] = "launchInStandaloneWindow";
inline constexpr char kAppTitle[] = "appTitle";
inline constexpr char kUrl[] = "url";
inline constexpr char kIconUrl[] = "iconPath";

std::unique_ptr<web_app::WebAppInstallInfo> GetAppInstallInfo(
    const base::Value::Dict& entry) {
  std::optional<bool> launch_in_standalone_window =
      entry.FindBool(kLaunchInStandaloneWindow);
  auto* app_title = entry.FindString(kAppTitle);
  auto* url = entry.FindString(kUrl);
  auto* icon_url = entry.FindString(kIconUrl);

  // Ensure that the required fields are present.
  if (!launch_in_standalone_window.has_value() || !app_title || !url) {
    return nullptr;
  }

  GURL url_parsed = GURL(*url);
  if (!url_parsed.is_valid()) {
    return nullptr;
  }

  // Campaigns don't specify a `manifest_id`, so each unique `start_url` will be
  // treated as a unique app.
  webapps::ManifestId manifest_id =
      web_app::GenerateManifestIdFromStartUrlOnly(url_parsed);
  auto info =
      std::make_unique<web_app::WebAppInstallInfo>(manifest_id, url_parsed);
  info->title = base::UTF8ToUTF16(*app_title);
  if (icon_url && GURL(*icon_url).is_valid()) {
    info->manifest_icons.push_back(apps::IconInfo(GURL(*icon_url), 32));
  }

  info->display_mode = launch_in_standalone_window.value()
                           ? blink::mojom::DisplayMode::kStandalone
                           : blink::mojom::DisplayMode::kBrowser;
  return info;
}

std::unique_ptr<web_app::WebAppInstallInfo>
ParseInstallWebAppActionPerformerParams(const base::Value::Dict* params) {
  if (!params) {
    CAMPAIGNS_LOG(ERROR) << "Empty parameter to InstallWebAction.";
    return nullptr;
  }

  return GetAppInstallInfo(*params);
}

void InstallWebAppResult(growth::ActionPerformer::Callback callback,
                         const std::string& app_id,
                         webapps::InstallResultCode code) {
  const std::array<webapps::InstallResultCode, 4> success_codes = {
      webapps::InstallResultCode::kSuccessNewInstall,
      webapps::InstallResultCode::kSuccessAlreadyInstalled,
      webapps::InstallResultCode::kSuccessOfflineOnlyInstall,
      webapps::InstallResultCode::kSuccessOfflineFallbackInstall};

  if (std::find(success_codes.begin(), success_codes.end(), code) !=
      success_codes.end()) {
    std::move(callback).Run(growth::ActionResult::kSuccess,
                            /* action_result_reason= */ std::nullopt);
    return;
  }

  // TODO(b/306023057): Record an UMA metric regarding why
  // we were not able to successfully record the app.
  std::move(callback).Run(
      growth::ActionResult::kFailure,
      growth::ActionResultReason::kWebAppInstallFailedOther);
}

void InstallWebAppImpl(web_app::WebAppProvider& provider,
                       std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
                       growth::ActionPerformer::Callback callback) {
  provider.scheduler().InstallFromInfoWithParams(
      std::move(web_app_info), /* overwrite_existing_manifest_fields= */ true,
      webapps::WebappInstallSource::EXTERNAL_DEFAULT,
      base::BindOnce(&InstallWebAppResult, std::move(callback)),
      web_app::WebAppInstallParams());
}

web_app::WebAppProvider* GetWebAppProvider() {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  return web_app::WebAppProvider::GetForWebApps(profile);
}

void TriggerWebAppInstall(std::unique_ptr<web_app::WebAppInstallInfo> info,
                          growth::ActionPerformer::Callback callback) {
  auto* provider = GetWebAppProvider();
  CHECK(provider);
  provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(InstallWebAppImpl, std::ref(*provider),
                                std::move(info), std::move(callback)));
}

}  // namespace

InstallWebAppActionPerformer::InstallWebAppActionPerformer() = default;
InstallWebAppActionPerformer::~InstallWebAppActionPerformer() = default;

void InstallWebAppActionPerformer::Run(
    int campaign_id,
    std::optional<int> group_id,
    const base::Value::Dict* params,
    growth::ActionPerformer::Callback callback) {
  if (!GetWebAppProvider()) {
    std::move(callback).Run(
        growth::ActionResult::kFailure,
        growth::ActionResultReason::kWebAppProviderNotAvailable);
    return;
  }

  auto info = ParseInstallWebAppActionPerformerParams(params);
  if (!info) {
    // TODO(b/306023057): Record an UMA metric that parsing the params
    // has failed.
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    return;
  }

  TriggerWebAppInstall(std::move(info), std::move(callback));
}

growth::ActionType InstallWebAppActionPerformer::ActionType() const {
  return growth::ActionType::kInstallWebApp;
}
