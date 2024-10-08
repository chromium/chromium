// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/web_app_profile_switcher.h"

#include <memory>
#include <optional>

#include "ash/constants/web_app_id_constants.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/browser_thread.h"

namespace {

std::unique_ptr<web_app::WebAppInstallInfo> MakeInstallInfoFromApp(
    const web_app::WebApp& web_app) {
  auto install_info = std::make_unique<web_app::WebAppInstallInfo>(
      web_app.manifest_id(), web_app.start_url());
  install_info->title = base::UTF8ToUTF16(web_app.untranslated_name());
  install_info->description =
      base::UTF8ToUTF16(web_app.untranslated_description());
  install_info->manifest_url = web_app.manifest_url();
  install_info->scope = web_app.scope();
  install_info->manifest_icons = web_app.manifest_icons();
  install_info->display_mode = web_app.display_mode();
  return install_info;
}

}  // namespace

WebAppProfileSwitcher::WebAppProfileSwitcher(const webapps::AppId& app_id,
                                             Profile& active_profile,
                                             base::OnceClosure on_completion)
    : app_id_(app_id),
      active_profile_(active_profile),
      on_completion_(std::move(on_completion)),
      weak_factory_(this) {
  profiles_observation_.AddObservation(&active_profile);
}

WebAppProfileSwitcher::~WebAppProfileSwitcher() = default;

void WebAppProfileSwitcher::SwitchToProfile(
    const base::FilePath& profile_to_open) {
  base::OnceCallback<void(Profile*)> open_web_app_callback = base::BindOnce(
      &WebAppProfileSwitcher::QueryProfileWebAppRegistryToOpenWebApp,
      weak_factory_.GetWeakPtr());
  profiles::LoadProfileAsync(profile_to_open, std::move(open_web_app_callback));

  if (app_id_ == web_app::kPasswordManagerAppId) {
    base::UmaHistogramEnumeration(
        "PasswordManager.ShortcutMetric",
        password_manager::metrics_util::PasswordManagerShortcutMetric::
            kProfileSwitched);
  }
}

void WebAppProfileSwitcher::OnProfileWillBeDestroyed(Profile* profile) {
  // If any of observed profiles is destroyed before the switch is completed,
  // the profile switcher should be destroyed.
  weak_factory_.InvalidateWeakPtrs();
  RunCompletionCallback();
}

void WebAppProfileSwitcher::QueryProfileWebAppRegistryToOpenWebApp(
    Profile* new_profile) {
  CHECK(!new_profile->IsGuestSession());
  new_profile_ = new_profile;
  profiles_observation_.AddObservation(new_profile);

  auto* provider = web_app::WebAppProvider::GetForWebApps(new_profile);
  CHECK(provider);
  provider->scheduler().ScheduleCallback(
      "QueryProfileWebAppRegistryToOpenWebApp",
      web_app::AppLockDescription(app_id_),
      base::BindOnce(
          &WebAppProfileSwitcher::InstallOrOpenWebAppWindowForProfile,
          weak_factory_.GetWeakPtr()),
      /*on_complete=*/base::DoNothing());
}

void WebAppProfileSwitcher::InstallOrOpenWebAppWindowForProfile(
    web_app::AppLock& new_profile_lock,
    base::Value::Dict& debug_value) {
  if (new_profile_lock.registrar().IsInstalled(app_id_)) {
    // The web app is already installed and can be launched, or foregrounded,
    // if it's already launched.
    Browser* launched_app =
        web_app::AppBrowserController::FindForWebApp(*new_profile_, app_id_);
    debug_value.Set("launched_app", !!launched_app);
    if (launched_app) {
      launched_app->window()->Activate();
      RunCompletionCallback();
    } else {
      LaunchAppWithId(app_id_,
                      webapps::InstallResultCode::kSuccessAlreadyInstalled);
    }
    return;
  }
  // Fetch app icons from the already installed app prior to
  // installation.
  // TODO(crbug.com/40256076) Use the icon loading command once it's available.
  web_app::WebAppProvider::GetForWebApps(&active_profile_.get())
      ->icon_manager()
      .ReadAllIcons(app_id_, base::BindOnce(
                                 &WebAppProfileSwitcher::InstallAndLaunchWebApp,
                                 weak_factory_.GetWeakPtr()));
}

void WebAppProfileSwitcher::InstallAndLaunchWebApp(
    web_app::IconBitmaps icon_bitmaps) {
  web_app::WebAppProvider* active_profile_provider =
      web_app::WebAppProvider::GetForWebApps(&active_profile_.get());
  if (!active_profile_provider->registrar_unsafe().IsInstalled(app_id_)) {
    RunCompletionCallback();
    return;
  }

  const web_app::WebApp* web_app =
      active_profile_provider->registrar_unsafe().GetAppById(app_id_);
  DCHECK(web_app);
  auto install_info = MakeInstallInfoFromApp(*web_app);
  install_info->icon_bitmaps = std::move(icon_bitmaps);

  web_app::WebAppInstallParams install_params;
  install_params.add_to_desktop = true;
  install_params.add_to_quick_launch_bar = true;
  install_params.add_to_applications_menu = true;
  install_params.user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  auto* provider = web_app::WebAppProvider::GetForWebApps(new_profile_);
  CHECK(provider);
  provider->scheduler().InstallFromInfoWithParams(
      std::move(install_info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::PROFILE_MENU,
      base::BindOnce(&WebAppProfileSwitcher::LaunchAppWithId,
                     weak_factory_.GetWeakPtr()),
      install_params);
}

void WebAppProfileSwitcher::LaunchAppWithId(
    const webapps::AppId& app_id,
    webapps::InstallResultCode install_result) {
  // TODO(crbug.com/40256076): Record metrics for installation failures.
  if (!IsSuccess(install_result)) {
    RunCompletionCallback();
    return;
  }

  web_app::WebAppProvider::GetForWebApps(new_profile_)
      ->scheduler()
      .LaunchApp(app_id, *base::CommandLine::ForCurrentProcess(),
                 /*current_directory=*/base::FilePath(),
                 /*url_handler_launch_url=*/std::nullopt,
                 /*protocol_handler_launch_url=*/std::nullopt,
                 /*file_launch_url=*/std::nullopt, /*launch_files=*/{},
                 base::IgnoreArgs<base::WeakPtr<Browser>,
                                  base::WeakPtr<content::WebContents>,
                                  apps::LaunchContainer>(base::BindOnce(
                     &WebAppProfileSwitcher::RunCompletionCallback,
                     weak_factory_.GetWeakPtr())));
}

void WebAppProfileSwitcher::RunCompletionCallback() {
  std::move(on_completion_).Run();
}
