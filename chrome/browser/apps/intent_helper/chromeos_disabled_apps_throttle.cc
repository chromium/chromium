// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/chromeos_disabled_apps_throttle.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace apps {
namespace {
using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

std::string GetAppDisabledErrorPage() {
  base::Value::Dict strings;

  strings.Set("disabledPageHeader",
              l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER));
  strings.Set("disabledPageTitle",
              l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_TITLE));
  strings.Set("disabledPageMessage",
              l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_MESSAGE));
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_CHROME_URLS_DISABLED_PAGE_HTML);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &strings);
  return webui::GetI18nTemplateHtml(html, strings);
}
}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
ChromeOsDisabledAppsThrottle::MaybeCreate(content::NavigationHandle* handle) {
  // Don't handle navigations in subframes or main frames that are in a nested
  // frame tree (e.g. portals, fenced-frame).
  if (!handle->IsInPrimaryMainFrame() && !handle->IsInPrerenderedMainFrame()) {
    return nullptr;
  }

  content::WebContents* web_contents = handle->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return nullptr;
  }

  return base::WrapUnique(new ChromeOsDisabledAppsThrottle(handle));
}

ChromeOsDisabledAppsThrottle::ChromeOsDisabledAppsThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}
ChromeOsDisabledAppsThrottle::~ChromeOsDisabledAppsThrottle() = default;

const char* ChromeOsDisabledAppsThrottle::GetNameForLogging() {
  return "ChromeOsDisabledAppsThrottle";
}

ThrottleCheckResult ChromeOsDisabledAppsThrottle::WillStartRequest() {
  return HandleRequest();
}

ThrottleCheckResult ChromeOsDisabledAppsThrottle::WillRedirectRequest() {
  return HandleRequest();
}

ThrottleCheckResult ChromeOsDisabledAppsThrottle::HandleRequest() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  const GURL& url = navigation_handle()->GetURL();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  std::vector<std::string> app_ids =
      apps::AppServiceProxyFactory::GetForProfile(profile)->GetAppIdsForUrl(
          url, /*exclude_browsers=*/true, /*exclude_browser_tab_apps=*/false);

  for (const auto& app_id : app_ids) {
    policy::SystemFeature system_feature =
        policy::SystemFeaturesDisableListPolicyHandler::
            GetSystemFeatureFromAppId(app_id);

    if (system_feature == policy::SystemFeature::kUnknownSystemFeature) {
      continue;
    }

    if (!policy::SystemFeaturesDisableListPolicyHandler::
            IsSystemFeatureDisabled(system_feature,
                                    g_browser_process->local_state())) {
      continue;
    }

    return ThrottleCheckResult(content::NavigationThrottle::CANCEL,
                               net::ERR_BLOCKED_BY_ADMINISTRATOR,
                               GetAppDisabledErrorPage());
  }
  return content::NavigationThrottle::PROCEED;
}

}  // namespace apps
