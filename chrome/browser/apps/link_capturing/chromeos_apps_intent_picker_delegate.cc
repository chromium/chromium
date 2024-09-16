// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/chromeos_apps_intent_picker_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser//web_applications/web_app_ui_manager.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/link_capturing/apps_intent_picker_delegate.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/apps/link_capturing/metrics/intent_handling_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"

namespace apps {

namespace {

PickerEntryType GetPickerEntryType(AppType app_type) {
  PickerEntryType picker_entry_type = PickerEntryType::kUnknown;
  switch (app_type) {
    case AppType::kUnknown:
    case AppType::kBuiltIn:
    case AppType::kCrostini:
    case AppType::kPluginVm:
    case AppType::kChromeApp:
    case AppType::kExtension:
    case AppType::kStandaloneBrowser:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kRemote:
    case AppType::kBorealis:
    case AppType::kBruschetta:
    case AppType::kStandaloneBrowserExtension:
      break;
    case AppType::kArc:
      picker_entry_type = PickerEntryType::kArc;
      break;
    case AppType::kWeb:
    case AppType::kSystemWeb:
      picker_entry_type = PickerEntryType::kWeb;
      break;
  }
  return picker_entry_type;
}

void CloseOrGoBack(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (web_contents->GetController().CanGoBack()) {
    web_contents->GetController().GoBack();
  } else {
    web_contents->ClosePage();
  }
}

}  // namespace

ChromeOsAppsIntentPickerDelegate::ChromeOsAppsIntentPickerDelegate(
    Profile* profile)
    : profile_(*profile) {
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile);
  }
}

ChromeOsAppsIntentPickerDelegate::~ChromeOsAppsIntentPickerDelegate() = default;

bool ChromeOsAppsIntentPickerDelegate::ShouldShowIntentPickerWithApps() {
  if (!web_app::AreWebAppsUserInstallable(&profile_.get())) {
    return false;
  }

  return apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      &profile_.get());
}

void ChromeOsAppsIntentPickerDelegate::FindAllAppsForUrl(
    const GURL& url,
    IntentPickerAppsCallback apps_callback) {
  CHECK(&profile_.get());
  std::vector<apps::IntentPickerAppInfo> apps;

  CHECK(proxy_);

  std::vector<std::string> app_ids =
      proxy_->GetAppIdsForUrl(url, /*exclude_browsers=*/true);

  for (const std::string& app_id : app_ids) {
    proxy_->AppRegistryCache().ForOneApp(
        app_id, [&apps](const apps::AppUpdate& update) {
          apps.emplace_back(GetPickerEntryType(update.AppType()),
                            ui::ImageModel(), update.AppId(), update.Name());
        });
  }
  // Reverse to keep old behavior of ordering (even though it was arbitrary, it
  // was at least deterministic).
  std::reverse(apps.begin(), apps.end());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(apps_callback), std::move(apps)));
}

bool ChromeOsAppsIntentPickerDelegate::IsPreferredAppForSupportedLinks(
    const std::string& app_id) {
  CHECK(proxy_);
  return proxy_->PreferredAppsList().IsPreferredAppForSupportedLinks(app_id);
}

void ChromeOsAppsIntentPickerDelegate::LoadSingleAppIcon(
    PickerEntryType entry_type,
    const std::string& app_id,
    int size_in_dep,
    IconLoadedCallback icon_loaded_callback) {
  CHECK(proxy_);

  auto transform_icon_to_metadata =
      base::BindOnce([](apps::IconValuePtr icon_ptr) -> ui::ImageModel {
        bool is_valid_icon =
            (icon_ptr && icon_ptr->icon_type == apps::IconType::kStandard);
        if (!is_valid_icon) {
          return ui::ImageModel();
        }

        return ui::ImageModel::FromImageSkia(icon_ptr->uncompressed);
      });
  proxy_->LoadIcon(app_id, apps::IconType::kStandard, size_in_dep,
                   /*allow_placeholder_icon=*/false,
                   std::move(transform_icon_to_metadata)
                       .Then(std::move(icon_loaded_callback)));
}

void ChromeOsAppsIntentPickerDelegate::RecordIntentPickerIconEvent(
    apps::IntentPickerIconEvent event) {
  base::UmaHistogramEnumeration("ChromeOS.Intents.IntentPickerIconEvent",
                                event);
}

bool ChromeOsAppsIntentPickerDelegate::ShouldLaunchAppDirectly(
    const GURL& url,
    const std::string& app_name,
    PickerEntryType) {
  // If there is only a single available app, immediately launch it if
  // ShouldShowLinkCapturingUX() is enabled and the app is preferred for this
  // link.
  CHECK(proxy_);
  return apps::features::ShouldShowLinkCapturingUX() &&
         (proxy_->PreferredAppsList().FindPreferredAppForUrl(url) == app_name);
}

void ChromeOsAppsIntentPickerDelegate::RecordOutputMetrics(
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist,
    bool should_launch_app) {
  apps::IntentHandlingMetrics::RecordIntentPickerMetrics(
      entry_type, close_reason, should_persist);

  if (should_persist) {
    apps::IntentHandlingMetrics::RecordLinkCapturingEvent(
        entry_type,
        apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged);
  }

  if (should_launch_app) {
    apps::IntentHandlingMetrics::RecordLinkCapturingEvent(
        entry_type,
        apps::IntentHandlingMetrics::LinkCapturingEvent::kAppOpened);
  }
}

void ChromeOsAppsIntentPickerDelegate::PersistIntentPreferencesForApp(
    PickerEntryType entry_type,
    const std::string& app_id) {
  CHECK(proxy_);
  CHECK(!app_id.empty());
  proxy_->SetSupportedLinksPreference(app_id);
}

void ChromeOsAppsIntentPickerDelegate::LaunchApp(
    content::WebContents* web_contents,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type) {
  CHECK(!launch_name.empty());
  if (entry_type == PickerEntryType::kWeb) {
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForWebApps(&profile_.get());
    provider->ui_manager().ReparentAppTabToWindow(web_contents, launch_name,
                                                  base::DoNothing());
  } else {
    CHECK(proxy_);
    proxy_->LaunchAppWithUrl(
        launch_name,
        GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                      /*prefer_container=*/true),
        url, LaunchSource::kFromLink,
        std::make_unique<WindowInfo>(display::kDefaultDisplayId));
    CloseOrGoBack(web_contents);
  }
}

}  // namespace apps
