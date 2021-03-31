// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/intent_helper/common_apps_navigation_throttle.h"

#include <utility>

#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/ash/apps/intent_helper/ash_intent_picker_helpers.h"
#include "chrome/browser/ash/apps/metrics/intent_handling_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/types/display_constants.h"

#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/resource/resource_bundle.h"

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

std::string GetAppDisabledErrorPage() {
  base::DictionaryValue strings;

  strings.SetString(
      "disabledPageHeader",
      l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER));
  strings.SetString(
      "disabledPageTitle",
      l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_TITLE));
  strings.SetString(
      "disabledPageMessage",
      l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_MESSAGE));
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_CHROME_URLS_DISABLED_PAGE_HTML);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &strings);
  return webui::GetI18nTemplateHtml(html, &strings);
}

bool IsAppDisabled(const std::string& app_id) {
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state)
    return false;

  const base::ListValue* disabled_system_features_pref =
      local_state->GetList(policy::policy_prefs::kSystemFeaturesDisableList);
  if (!disabled_system_features_pref)
    return false;

  policy::SystemFeature system_feature =
      policy::SystemFeaturesDisableListPolicyHandler::GetSystemFeatureFromAppId(
          app_id);
  if (system_feature == policy::SystemFeature::kUnknownSystemFeature)
    return false;

  const auto disabled_system_features =
      disabled_system_features_pref->GetList();
  return base::Contains(disabled_system_features, base::Value(system_feature));
}

}  // namespace

// static
std::unique_ptr<apps::AppsNavigationThrottle>
CommonAppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  content::WebContents* web_contents = handle->GetWebContents();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return nullptr;

  if (!ShouldCheckAppsForUrl(web_contents))
    return nullptr;

  return std::make_unique<CommonAppsNavigationThrottle>(handle);
}

CommonAppsNavigationThrottle::CommonAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : apps::AppsNavigationThrottle(navigation_handle) {}

CommonAppsNavigationThrottle::~CommonAppsNavigationThrottle() = default;

bool CommonAppsNavigationThrottle::ShouldCancelNavigation(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();

  const GURL& url = handle->GetURL();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  std::vector<std::string> app_ids =
      proxy->GetAppIdsForUrl(url, /*exclude_browser=*/true);

  if (app_ids.empty())
    return false;

  if (!navigate_from_link())
    return false;

  base::Optional<std::string> preferred_app_id =
      proxy->PreferredApps().FindPreferredAppForUrl(url);
  if (!preferred_app_id.has_value() ||
      !base::Contains(app_ids, preferred_app_id.value())) {
    return false;
  }

  // Only automatically launch PWA if the flag is on.
  apps::mojom::AppType app_type =
      proxy->AppRegistryCache().GetAppType(preferred_app_id.value());
  if (app_type != apps::mojom::AppType::kArc &&
      (app_type != apps::mojom::AppType::kWeb ||
       !base::FeatureList::IsEnabled(features::kIntentPickerPWAPersistence))) {
    return false;
  }

  // Don't capture if already inside the target app scope.
  if (app_type == apps::mojom::AppType::kWeb) {
    auto* tab_helper =
        web_app::WebAppTabHelperBase::FromWebContents(web_contents);
    if (tab_helper && tab_helper->GetAppId() == preferred_app_id.value())
      return false;
  }

  auto launch_source = apps::mojom::LaunchSource::kFromLink;
  proxy->LaunchAppWithUrl(
      preferred_app_id.value(),
      GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                    WindowOpenDisposition::NEW_WINDOW,
                    /*prefer_container=*/true),
      url, launch_source, apps::MakeWindowInfo(display::kDefaultDisplayId));

  if (web_contents->GetVisibleURL().IsAboutBlank())
    web_contents->ClosePage();

  IntentHandlingMetrics::RecordIntentPickerUserInteractionMetrics(
      /*selected_app_package=*/preferred_app_id.value(),
      GetPickerEntryType(app_type),
      apps::IntentPickerCloseReason::PREFERRED_APP_FOUND,
      apps::Source::kHttpOrHttps, /*should_persist=*/false);
  return true;
}

bool CommonAppsNavigationThrottle::ShouldShowDisablePage(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::vector<std::string> app_ids =
      apps::AppServiceProxyFactory::GetForProfile(profile)->GetAppIdsForUrl(
          url, /*exclude_browser=*/true);

  for (auto app_id : app_ids) {
    if (IsAppDisabled(app_id)) {
      return true;
    }
  }
  return false;
}

ThrottleCheckResult CommonAppsNavigationThrottle::MaybeShowCustomResult() {
  return ThrottleCheckResult(content::NavigationThrottle::CANCEL,
                             net::ERR_BLOCKED_BY_ADMINISTRATOR,
                             GetAppDisabledErrorPage());
}

}  // namespace apps
