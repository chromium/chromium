// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_navigation_throttle.h"

#include <string>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_error_page/web_time_limit_error_page.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace chromeos {

namespace {

bool IsWebBlocked(content::BrowserContext* context) {
  auto* child_user_service =
      ChildUserServiceFactory::GetForBrowserContext(context);
  DCHECK(child_user_service);

  return child_user_service->WebTimeLimitReached();
}

bool IsURLAllowlisted(const GURL& url, content::BrowserContext* context) {
  auto* child_user_service =
      ChildUserServiceFactory::GetForBrowserContext(context);
  if (!child_user_service)
    return false;

  return child_user_service->WebTimeLimitAllowlistedURL(url);
}

bool IsWebAppAllowlisted(const std::string& app_id_string,
                         content::BrowserContext* context) {
  const chromeos::app_time::AppId app_id(apps::mojom::AppType::kWeb,
                                         app_id_string);
  auto* child_user_service =
      ChildUserServiceFactory::GetForBrowserContext(context);
  DCHECK(child_user_service);
  return child_user_service->AppTimeLimitAllowlistedApp(app_id);
}

base::TimeDelta GetWebTimeLimit(content::BrowserContext* context) {
  auto* child_user_service =
      ChildUserServiceFactory::GetForBrowserContext(context);
  DCHECK(child_user_service);
  return child_user_service->GetWebTimeLimit();
}

Browser* FindBrowserForWebContents(content::WebContents* web_contents) {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); i++) {
      if (web_contents == tab_strip_model->GetWebContentsAt(i))
        return browser;
    }
  }

  return nullptr;
}

}  // namespace

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

// static
std::unique_ptr<WebTimeLimitNavigationThrottle>
WebTimeLimitNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  content::BrowserContext* browser_context =
      navigation_handle->GetWebContents()->GetBrowserContext();

  if (!Profile::FromBrowserContext(browser_context)->IsChild())
    return nullptr;

  if (!app_time::WebTimeLimitEnforcer::IsEnabled())
    return nullptr;

  if (!navigation_handle->IsInMainFrame())
    return nullptr;

  // Creating a throttle for both the main frame and sub frames. This prevents
  // kids from circumventing the app restrictions by using iframes in a local
  // html file.
  return IsWebBlocked(browser_context)
             ? base::WrapUnique(
                   new WebTimeLimitNavigationThrottle(navigation_handle))
             : nullptr;
}

WebTimeLimitNavigationThrottle::~WebTimeLimitNavigationThrottle() = default;

ThrottleCheckResult WebTimeLimitNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

ThrottleCheckResult WebTimeLimitNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

ThrottleCheckResult WebTimeLimitNavigationThrottle::WillProcessResponse() {
  return WillStartOrRedirectRequest();
}

const char* WebTimeLimitNavigationThrottle::GetNameForLogging() {
  return "WebTimeLimitNavigationThrottle";
}

WebTimeLimitNavigationThrottle::WebTimeLimitNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

ThrottleCheckResult
WebTimeLimitNavigationThrottle::WillStartOrRedirectRequest() {
  content::BrowserContext* browser_context =
      navigation_handle()->GetWebContents()->GetBrowserContext();

  if (!IsWebBlocked(browser_context) ||
      IsURLAllowlisted(navigation_handle()->GetURL(), browser_context)) {
    return NavigationThrottle::PROCEED;
  }

  // Let's get the browser instance from the navigation handle.
  content::WebContents* web_contents = navigation_handle()->GetWebContents();

  // Proceed if the |web_contents| is embedded in another WebContents or if it
  // is doing background work for another user facing WebContents.
  if (web_contents->GetOutermostWebContents() != web_contents ||
      web_contents->GetResponsibleWebContents() != web_contents) {
    return PROCEED;
  }

  Browser* browser = FindBrowserForWebContents(web_contents);

  if (!browser)
    return PROCEED;

  bool is_windowed = false;
  if (browser) {
    Browser::Type type = browser->type();
    is_windowed = (type == Browser::Type::TYPE_APP_POPUP) ||
                  (type == Browser::Type::TYPE_APP) ||
                  (type == Browser::Type::TYPE_POPUP);
  }

  web_app::WebAppTabHelperBase* web_app_helper =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents);

  bool is_app = web_app_helper && !web_app_helper->GetAppId().empty();

  base::TimeDelta time_limit = GetWebTimeLimit(browser_context);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();

  if (!is_app) {
    const GURL& url = navigation_handle()->GetURL();

    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        url, net::registry_controlled_domains::PrivateRegistryFilter::
                 INCLUDE_PRIVATE_REGISTRIES);

    if (domain.empty())
      domain = url.has_host() ? url.host() : url.spec();

    app_time::WebTimeNavigationObserver* observer =
        app_time::WebTimeNavigationObserver::FromWebContents(web_contents);
    const base::Optional<std::u16string>& prev_title =
        observer ? observer->previous_title() : base::nullopt;

    return NavigationThrottle::ThrottleCheckResult(
        CANCEL, net::ERR_BLOCKED_BY_CLIENT,
        GetWebTimeLimitChromeErrorPage(domain, prev_title, time_limit,
                                       app_locale));
  }

  // Don't throttle windowed applications. We show a notification and close
  // them.
  if (is_windowed)
    return PROCEED;

  //  Don't throttle allowlisted applications.
  if (IsWebAppAllowlisted(web_app_helper->GetAppId(), browser_context))
    return PROCEED;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::Get(profile);
  const web_app::AppRegistrar& registrar = web_app_provider->registrar();
  const std::string& app_name =
      registrar.GetAppShortName(web_app_helper->GetAppId());
  return NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT,
      GetWebTimeLimitAppErrorPage(time_limit, app_locale, app_name));
}

}  // namespace chromeos
