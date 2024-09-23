// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/media_app/media_app_guest_ui_config.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "base/version.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_handler_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/mahi/media_app/mahi_media_app_handler_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

bool PhotosIntegrationSupported(const apps::AppUpdate& update) {
  constexpr char kMinPhotosVersion[] = "6.12";
  if (!apps_util::IsInstalled(update.Readiness())) {
    return false;
  }

  auto photos_version = base::Version(update.Version());
  if (!photos_version.IsValid()) {
    // There are two reasons why a version string from Photos will be "invalid",
    // i.e. cannot be parsed into a concatenation of numbers and dots:
    // 1. The version string is "DEVELOPMENT", which is used by Photos engineers
    // for unreleased builds, or
    // 2. The version is suffixed with e.g. "-dogfood" or "-release", which is
    // the new standard for Photos version strings.
    // Version suffixes (2) were introduced a few months after version 6.12 was
    // released, so we can conclude "invalid" strings are post-6.12 and
    // therefore are supported for the Media App <-> Photos integration. It is
    // safer to assume "invalid" versions are supported rather than implementing
    // special handling of them to e.g. strip suffixes since there are no
    // guarantees the version strings won't change again in the future.
    return true;
  }

  return photos_version >= base::Version(kMinPhotosVersion);
}

bool IsLensInGalleryEnabled(Profile* profile, PrefService* pref_service) {
  if (!pref_service->GetBoolean(prefs::kMediaAppLensEnabled)) {
    return false;
  }

  const TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* default_url = service->GetDefaultSearchProvider();
  return default_url &&
         default_url->url_ref().HasGoogleBaseURLs(service->search_terms_data());
}

}  // namespace

ChromeMediaAppGuestUIDelegate::ChromeMediaAppGuestUIDelegate() = default;

void ChromeMediaAppGuestUIDelegate::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMediaAppLensEnabled, true);
}

void ChromeMediaAppGuestUIDelegate::PopulateLoadTimeData(
    content::WebUI* web_ui,
    content::WebUIDataSource* source) {
  Profile* profile = Profile::FromWebUI(web_ui);
  PrefService* pref_service = profile->GetPrefs();
  apps::AppRegistryCache& app_registry_cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  bool photos_integration_supported = false;
  app_registry_cache.ForOneApp(
      arc::kGooglePhotosAppId,
      [&photos_integration_supported](const apps::AppUpdate& update) {
        photos_integration_supported = PhotosIntegrationSupported(update);
      });

  source->AddString("appLocale", g_browser_process->GetApplicationLocale());
  source->AddBoolean("lensInGallery",
                     IsLensInGalleryEnabled(profile, pref_service));
  source->AddBoolean("pdfReadonly",
                     !pref_service->GetBoolean(prefs::kPdfAnnotationsEnabled));
  version_info::Channel channel = chrome::GetChannel();
  source->AddBoolean("colorThemes", true);
  source->AddBoolean("photosAvailableForImage", photos_integration_supported);
  source->AddBoolean("photosAvailableForVideo", photos_integration_supported);

  // TODO(b:356518781): rename the boolean to e.g. pdfContextMenu.
  // If true, show a context menu on right click on PDF surface. And if Mahi
  // message pipe is connected (see `CreateAndBindMahiHandler` below), also show
  // the Mahi card for the user to use HelpMeRead feature for the PDF.
  source->AddBoolean(
      "pdfMahi", base::FeatureList::IsEnabled(ash::features::kMediaAppPdfMahi));

  source->AddBoolean("flagsMenu", channel != version_info::Channel::BETA &&
                                      channel != version_info::Channel::STABLE);
  source->AddBoolean("isDevChannel", channel == version_info::Channel::DEV);
}

void ChromeMediaAppGuestUIDelegate::CreateAndBindOcrHandler(
    content::BrowserContext& context,
    gfx::NativeWindow native_window,
    mojo::PendingReceiver<ash::media_app_ui::mojom::OcrUntrustedPageHandler>
        receiver,
    mojo::PendingRemote<ash::media_app_ui::mojom::OcrUntrustedPage> page) {
  ash::AXMediaAppHandlerFactory::GetInstance()
      ->CreateAXMediaAppUntrustedHandler(context, native_window,
                                         std::move(receiver), std::move(page));
}

void ChromeMediaAppGuestUIDelegate::CreateAndBindMahiHandler(
    mojo::PendingReceiver<ash::media_app_ui::mojom::MahiUntrustedPageHandler>
        receiver,
    mojo::PendingRemote<ash::media_app_ui::mojom::MahiUntrustedPage> page,
    const std::string& file_name,
    aura::Window* window) {
  if (chromeos::MahiManager::Get() &&
      chromeos::MahiManager::Get()->IsEnabled()) {
    ash::MahiMediaAppHandlerFactory::GetInstance()
        ->CreateMahiMediaAppUntrustedHandler(
            std::move(receiver), std::move(page), file_name, window);
  }
}

MediaAppGuestUIConfig::MediaAppGuestUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  ash::kChromeUIMediaAppHost) {}

MediaAppGuestUIConfig::~MediaAppGuestUIConfig() = default;

std::unique_ptr<content::WebUIController>
MediaAppGuestUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                             const GURL& url) {
  auto delegate = std::make_unique<ChromeMediaAppGuestUIDelegate>();
  return std::make_unique<ash::MediaAppGuestUI>(web_ui, std::move(delegate));
}
