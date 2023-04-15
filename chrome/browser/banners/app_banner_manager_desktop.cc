// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/install_result_code.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// Platform values defined in:
// https://github.com/w3c/manifest/wiki/Platforms
const char kPlatformChromeWebStore[] = "chrome_web_store";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kPlatformPlay[] = "play";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

namespace webapps {

AppBannerManagerDesktop::CreateAppBannerManagerForTesting
    AppBannerManagerDesktop::override_app_banner_manager_desktop_for_testing_ =
        nullptr;

// static
void AppBannerManagerDesktop::CreateForWebContents(
    content::WebContents* web_contents) {
  if (FromWebContents(web_contents))
    return;

  if (override_app_banner_manager_desktop_for_testing_) {
    web_contents->SetUserData(
        UserDataKey(),
        override_app_banner_manager_desktop_for_testing_(web_contents));
    return;
  }
  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new AppBannerManagerDesktop(web_contents)));
}

TestAppBannerManagerDesktop*
AppBannerManagerDesktop::AsTestAppBannerManagerDesktopForTesting() {
  return nullptr;
}

AppBannerManagerDesktop::AppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents),
      content::WebContentsUserData<AppBannerManagerDesktop>(*web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  extension_registry_ = extensions::ExtensionRegistry::Get(profile);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  // May be null in unit tests e.g. TabDesktopMediaListTest.*.
  if (provider)
    install_manager_observation_.Observe(&provider->install_manager());
}

AppBannerManagerDesktop::~AppBannerManagerDesktop() = default;

base::WeakPtr<AppBannerManager> AppBannerManagerDesktop::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppBannerManagerDesktop::InvalidateWeakPtrs() {
  weak_factory_.InvalidateWeakPtrs();
}

bool AppBannerManagerDesktop::IsSupportedNonWebAppPlatform(
    const std::u16string& platform) const {
  if (base::EqualsASCII(platform, kPlatformChromeWebStore))
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::EqualsASCII(platform, kPlatformPlay) &&
      arc::IsArcAllowedForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()))) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return false;
}

bool AppBannerManagerDesktop::IsRelatedNonWebAppInstalled(
    const blink::Manifest::RelatedApplication& related_app) const {
  if (!related_app.id || related_app.id->empty() || !related_app.platform ||
      related_app.platform->empty()) {
    return false;
  }

  const std::string id = base::UTF16ToUTF8(*related_app.id);
  const std::u16string& platform = *related_app.platform;

  if (base::EqualsASCII(platform, kPlatformChromeWebStore)) {
    return extension_registry_->GetExtensionById(
               id, extensions::ExtensionRegistry::ENABLED) != nullptr;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::EqualsASCII(platform, kPlatformPlay)) {
    ArcAppListPrefs* arc_app_list_prefs =
        ArcAppListPrefs::Get(web_contents()->GetBrowserContext());
    return arc_app_list_prefs && arc_app_list_prefs->GetPackage(id) != nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return false;
}

bool AppBannerManagerDesktop::IsWebAppConsideredInstalled() const {
  return web_app::FindInstalledAppWithUrlInScope(
             Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
             manifest().start_url)
      .has_value();
}

std::string AppBannerManagerDesktop::GetAppIdentifier() {
  DCHECK(!blink::IsEmptyManifest(manifest()));
  return web_app::GenerateAppIdUnhashedFromManifest(manifest());
}

web_app::WebAppRegistrar& AppBannerManagerDesktop::registrar() {
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  DCHECK(provider);
  return provider->registrar_unsafe();
}

bool AppBannerManagerDesktop::ShouldAllowWebAppReplacementInstall() {
  // Only allow replacement install if this specific app is already installed.
  web_app::AppId app_id = web_app::GenerateAppIdFromManifest(manifest());
  if (!registrar().IsLocallyInstalled(app_id))
    return false;

  // We prompt the user to re-install if the site wants to be in a standalone
  // window but the user has opted for opening in browser tab. This is to
  // support the situation where a site is not a PWA, users have installed it
  // via Create Shortcut action, the site becomes a standalone PWA later and we
  // want to prompt them to "install" the new PWA experience.
  // TODO(crbug.com/1205529): Showing an install button when it's already
  // installed is confusing.
  auto display_mode = registrar().GetAppUserDisplayMode(app_id);
  return display_mode == web_app::mojom::UserDisplayMode::kBrowser;
}

void AppBannerManagerDesktop::ShowBannerUi(WebappInstallSource install_source) {
  RecordDidShowBanner();
  TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
  ReportStatus(SHOWING_APP_INSTALLATION_DIALOG);
  CreateWebApp(install_source);
}

void AppBannerManagerDesktop::OnWebAppInstalled(
    const web_app::AppId& installed_app_id) {
  absl::optional<web_app::AppId> app_id =
      registrar().FindAppWithUrlInScope(validated_url_);
  if (app_id.has_value() && *app_id == installed_app_id &&
      registrar().GetAppUserDisplayMode(*app_id) ==
          web_app::mojom::UserDisplayMode::kStandalone) {
    OnInstall(registrar().GetEffectiveDisplayModeFromManifest(*app_id));
    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
  }
}

void AppBannerManagerDesktop::OnWebAppWillBeUninstalled(
    const web_app::AppId& app_id) {
  // WebAppTabHelper has a app_id but it is reset during
  // OnWebAppWillBeUninstalled so use IsUrlInAppScope() instead.
  if (registrar().IsUrlInAppScope(validated_url(), app_id))
    uninstalling_app_id_ = app_id;
}

void AppBannerManagerDesktop::OnWebAppUninstalled(
    const web_app::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  if (uninstalling_app_id_ == app_id)
    RecheckInstallabilityForLoadedPage(validated_url(), true);
}

void AppBannerManagerDesktop::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void AppBannerManagerDesktop::CreateWebApp(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  web_app::CreateWebAppFromManifest(
      contents, /*bypass_service_worker_check=*/false, install_source,
      base::BindOnce(&AppBannerManagerDesktop::DidFinishCreatingWebApp,
                     weak_factory_.GetWeakPtr()));
}

void AppBannerManagerDesktop::DidFinishCreatingWebApp(
    const web_app::AppId& app_id,
    webapps::InstallResultCode code) {
  content::WebContents* contents = web_contents();
  if (!contents)
    return;

  // Catch only kSuccessNewInstall and kUserInstallDeclined. Report nothing on
  // all other errors.
  if (code == webapps::InstallResultCode::kSuccessNewInstall) {
    SendBannerAccepted();
    TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
    AppBannerSettingsHelper::RecordBannerInstallEvent(contents,
                                                      GetAppIdentifier());
  } else if (code == webapps::InstallResultCode::kUserInstallDeclined) {
    SendBannerDismissed();
    TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(contents,
                                                      GetAppIdentifier());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppBannerManagerDesktop);

}  // namespace webapps
