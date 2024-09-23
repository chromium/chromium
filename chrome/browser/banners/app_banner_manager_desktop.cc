// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
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
  if (provider) {
    install_manager_observation_.Observe(&provider->install_manager());
  }
}

AppBannerManagerDesktop::~AppBannerManagerDesktop() = default;

bool AppBannerManagerDesktop::CanRequestAppBanner() const {
  return true;
}

InstallableParams
AppBannerManagerDesktop::ParamsToPerformInstallableWebAppCheck() {
  InstallableParams params;
  params.valid_primary_icon = true;
  params.fetch_screenshots = true;
  params.installable_criteria = InstallableCriteria::kValidManifestWithIcons;
  return params;
}

bool AppBannerManagerDesktop::ShouldDoNativeAppCheck(
    const blink::mojom::Manifest& manifest) const {
  return false;
}

void AppBannerManagerDesktop::DoNativeAppInstallableCheck(
    content::WebContents* web_contents,
    const GURL& validated_url,
    const blink::mojom::Manifest& manifest,
    NativeCheckCallback callback) {
  NOTREACHED();
}

void AppBannerManagerDesktop::OnWebAppInstallableCheckedNoErrors(
    const ManifestId& manifest_id) const {}

base::expected<void, InstallableStatusCode>
AppBannerManagerDesktop::CanRunWebAppInstallableChecks(
    const blink::mojom::Manifest& manifest) {
  return base::ok();
}

base::WeakPtr<AppBannerManager>
AppBannerManagerDesktop::GetWeakPtrForThisNavigation() {
  return weak_factory_.GetWeakPtr();
}

void AppBannerManagerDesktop::InvalidateWeakPtrsForThisNavigation() {
  weak_factory_.InvalidateWeakPtrs();
}
void AppBannerManagerDesktop::ResetCurrentPageData() {}

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
    return extension_registry_->enabled_extensions().Contains(id);
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

void AppBannerManagerDesktop::MaybeShowAmbientBadge(
    const InstallBannerConfig& config) {}

void AppBannerManagerDesktop::OnMlInstallPrediction(
    base::PassKey<MLInstallabilityPromoter>,
    std::string result_label) {
  if (result_label == MLInstallabilityPromoter::kShowInstallPromptLabel) {
    CreateWebApp(
        WebappInstallSource::ML_PROMOTION,
        base::BindOnce(&AppBannerManagerDesktop::DidCreateWebAppFromMLDialog,
                       weak_factory_.GetWeakPtr()));
  }
}

web_app::WebAppRegistrar& AppBannerManagerDesktop::registrar() const {
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  DCHECK(provider);
  return provider->registrar_unsafe();
}

void AppBannerManagerDesktop::ShowBannerUi(WebappInstallSource install_source,
                                           const InstallBannerConfig& config) {
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), config,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, GetCurrentTime());
  TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
  CreateWebApp(install_source,
               base::BindOnce(&AppBannerManagerDesktop::DidFinishCreatingWebApp,
                              weak_factory_.GetWeakPtr(),
                              config.web_app_data.manifest().id,
                              weak_factory_.GetWeakPtr()));
}

void AppBannerManagerDesktop::OnWebAppInstalledWithOsHooks(
    const webapps::AppId& installed_app_id) {
  if (!validated_url()) {
    return;
  }
  std::optional<webapps::AppId> app_id =
      registrar().FindAppWithUrlInScope(validated_url().value());
  if (app_id.has_value() && *app_id == installed_app_id &&
      registrar().GetAppUserDisplayMode(*app_id) ==
          web_app::mojom::UserDisplayMode::kStandalone) {
    OnInstall(registrar().GetEffectiveDisplayModeFromManifest(*app_id),
              /*set_current_web_app_not_installable=*/true);
  }
}

void AppBannerManagerDesktop::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  if (!validated_url()) {
    return;
  }
  // WebAppTabHelper has a app_id but it is reset during
  // OnWebAppWillBeUninstalled so use IsUrlInAppScope() instead.
  if (registrar().IsUrlInAppScope(validated_url().value(), app_id)) {
    uninstalling_app_id_ = app_id;
  }
}

void AppBannerManagerDesktop::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  if (uninstalling_app_id_ == app_id) {
    RecheckInstallabilityForLoadedPage();
  }
}

void AppBannerManagerDesktop::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void AppBannerManagerDesktop::CreateWebApp(
    WebappInstallSource install_source,
    web_app::WebAppInstalledCallback install_callback) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  web_app::CreateWebAppFromManifest(contents, install_source,
                                    std::move(install_callback));
}

void AppBannerManagerDesktop::DidFinishCreatingWebApp(
    const webapps::ManifestId& manifest_id,
    base::WeakPtr<AppBannerManagerDesktop> is_navigation_current,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  content::WebContents* contents = web_contents();
  if (!contents)
    return;

  // Catch only kSuccessNewInstall and kUserInstallDeclined. Report nothing on
  // all other errors.
  if (code == webapps::InstallResultCode::kSuccessNewInstall) {
    if (is_navigation_current) {
      SendBannerAccepted();
    }
    TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
    AppBannerSettingsHelper::RecordBannerInstallEvent(contents,
                                                      manifest_id.spec());
  } else if (code == webapps::InstallResultCode::kUserInstallDeclined) {
    if (is_navigation_current) {
      SendBannerDismissed();
    }
    TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(contents,
                                                      manifest_id.spec());
  }
}

void AppBannerManagerDesktop::DidCreateWebAppFromMLDialog(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  if (code == webapps::InstallResultCode::kSuccessNewInstall) {
    TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
  } else if (code == webapps::InstallResultCode::kUserInstallDeclined) {
    TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppBannerManagerDesktop);

}  // namespace webapps
