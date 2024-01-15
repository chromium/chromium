// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_url_handling.h"

#include "base/check_is_test.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "url/gurl.h"

namespace lacros_url_handling {

bool IsNavigationInterceptable(const NavigateParams& params,
                               const GURL& source_url) {
  const auto qualifier = PageTransitionGetQualifier(params.transition);
  // True if this was triggered by the user typing into the Omnibox or via
  // crosapi (see BrowserServiceLacros::OpenUrlImpl).
  const bool is_omnibox_or_crosapi_navigation =
      (PageTransitionCoreTypeIs(params.transition, ui::PAGE_TRANSITION_TYPED) ||
       PageTransitionCoreTypeIs(params.transition,
                                ui::PAGE_TRANSITION_GENERATED)) &&
      qualifier & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  // True if this is a bookmark navigation.
  const bool is_bookmark_navigation = PageTransitionCoreTypeIs(
      params.transition, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  // True if this is a navigation created by the user, clicking on a link on a
  // page with the chrome:// scheme.
  const bool is_system_navigation =
      PageTransitionCoreTypeIs(params.transition, ui::PAGE_TRANSITION_LINK) &&
      source_url.SchemeIs(content::kChromeUIScheme);
  return (is_omnibox_or_crosapi_navigation || is_system_navigation ||
          is_bookmark_navigation);
}

bool MaybeInterceptNavigation(const GURL& url) {
  // Every URL which is supported by Ash but not by Lacros will automatically be
  // forwarded to Ash.
  return !IsUrlHandledByLacros(url) &&
         NavigateInAsh(
             crosapi::gurl_os_handler_utils::GetAshUrlFromLacrosUrl(url));
}

bool IsUrlHandledByLacros(const GURL& url) {
  return ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(url);
}

bool IsUrlAcceptedByAsh(const GURL& url) {
  auto* init_params = chromeos::BrowserParamsProxy::Get();
  const std::optional<std::vector<GURL>>& accepted_urls =
      init_params->AcceptedInternalAshUrls();
  if (!accepted_urls.has_value()) {
    CHECK_IS_TEST();
    return false;
  }

  return crosapi::gurl_os_handler_utils::IsAshUrlInList(url, *accepted_urls);
}

bool NavigateInAsh(GURL url) {
  if (!IsUrlAcceptedByAsh(url)) {
    return false;
  }

  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::UrlHandler>()) {
    return false;
  }

  service->GetRemote<crosapi::mojom::UrlHandler>()->OpenUrl(url);
  return true;
}

}  // namespace lacros_url_handling
