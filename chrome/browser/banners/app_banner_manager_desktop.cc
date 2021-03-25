// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// Platform values defined in:
// https://github.com/w3c/manifest/wiki/Platforms
const char kPlatformChromeWebStore[] = "chrome_web_store";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kPlatformPlay[] = "play";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool gDisableTriggeringForTesting = false;

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

void AppBannerManagerDesktop::DisableTriggeringForTesting() {
  gDisableTriggeringForTesting = true;
}

TestAppBannerManagerDesktop*
AppBannerManagerDesktop::AsTestAppBannerManagerDesktopForTesting() {
  return nullptr;
}

AppBannerManagerDesktop::AppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  extension_registry_ = extensions::ExtensionRegistry::Get(profile);
  auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile);
  // May be null in unit tests e.g. TabDesktopMediaListTest.*.
  if (provider)
    registrar_observer_.Add(&provider->registrar());
}

AppBannerManagerDesktop::~AppBannerManagerDesktop() { }

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
             manifest_.start_url)
      .has_value();
}

web_app::AppRegistrar& AppBannerManagerDesktop::registrar() {
  auto* provider = web_app::WebAppProviderBase::GetProviderBase(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  DCHECK(provider);
  return provider->registrar();
}

bool AppBannerManagerDesktop::ShouldAllowWebAppReplacementInstall() {
  // Only allow replacement install if this specific app is already installed.
  web_app::AppId app_id = web_app::GenerateAppIdFromURL(manifest_.start_url);
  if (!registrar().IsLocallyInstalled(app_id))
    return false;

  auto display_mode = registrar().GetAppUserDisplayMode(app_id);
  return display_mode == blink::mojom::DisplayMode::kBrowser;
}

void AppBannerManagerDesktop::ShowBannerUi(WebappInstallSource install_source) {
  RecordDidShowBanner();
  TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
  ReportStatus(SHOWING_APP_INSTALLATION_DIALOG);
  CreateWebApp(install_source);
}

void AppBannerManagerDesktop::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (gDisableTriggeringForTesting)
    return;

  AppBannerManager::DidFinishLoad(render_frame_host, validated_url);
}

void AppBannerManagerDesktop::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& url,
    double score,
    site_engagement::EngagementType type) {
  if (gDisableTriggeringForTesting)
    return;

  AppBannerManager::OnEngagementEvent(web_contents, url, score, type);
}

void AppBannerManagerDesktop::OnWebAppInstalled(
    const web_app::AppId& installed_app_id) {
  base::Optional<web_app::AppId> app_id =
      registrar().FindAppWithUrlInScope(validated_url_);
  if (app_id.has_value() && *app_id == installed_app_id &&
      registrar().GetAppUserDisplayMode(*app_id) ==
          blink::mojom::DisplayMode::kStandalone) {
    OnInstall(registrar().GetEffectiveDisplayModeFromManifest(*app_id));
    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
  }
}

void AppBannerManagerDesktop::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
}

void AppBannerManagerDesktop::CreateWebApp(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  // TODO(loyso): Take appropriate action if WebApps disabled for profile.
  web_app::CreateWebAppFromManifest(
      contents, /*bypass_service_worker_check=*/false, install_source,
      base::BindOnce(&AppBannerManagerDesktop::DidFinishCreatingWebApp,
                     weak_factory_.GetWeakPtr()));
}

void AppBannerManagerDesktop::DidFinishCreatingWebApp(
    const web_app::AppId& app_id,
    web_app::InstallResultCode code) {
  content::WebContents* contents = web_contents();
  if (!contents)
    return;

  // Catch only kSuccessNewInstall and kUserInstallDeclined. Report nothing on
  // all other errors.
  if (code == web_app::InstallResultCode::kSuccessNewInstall) {
    SendBannerAccepted();
    TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
    AppBannerSettingsHelper::RecordBannerInstallEvent(contents,
                                                      GetAppIdentifier());
  } else if (code == web_app::InstallResultCode::kUserInstallDeclined) {
    SendBannerDismissed();
    TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(contents,
                                                      GetAppIdentifier());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppBannerManagerDesktop)

}  // namespace webapps
