// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_url_handling.h"

#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "url/gurl.h"

namespace lacros_url_handling {

bool MaybeInterceptNavigation(const GURL& url) {
  const GURL& ash_url = crosapi::gurl_os_handler_utils::SanitizeAshURL(url);
  // Every URL which is supported by Ash but not by Lacros will automatically
  // forwarded to Ash.
  if (IsUrlHandledByLacros(ash_url) || !IsUrlAcceptedByAsh(ash_url))
    return false;

  return NavigateInAsh(ash_url);
}

bool IsUrlHandledByLacros(const GURL& url) {
  return ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(url);
}

bool IsUrlAcceptedByAsh(const GURL& requested_url) {
  auto* init_params = chromeos::LacrosService::Get()->init_params();
  if (!init_params->accepted_internal_ash_urls.has_value()) {
    // For Ash backwards compatibility allow URLs to be used which were
    // allowed before crosapi passed allowed URLs.
    return requested_url == GURL(chrome::kChromeUIOSSettingsURL)
                                .DeprecatedGetOriginAsURL() ||
           requested_url ==
               GURL(chrome::kChromeUIFlagsURL).DeprecatedGetOriginAsURL();
  }

  return crosapi::gurl_os_handler_utils::IsUrlInList(
      requested_url, *init_params->accepted_internal_ash_urls);
}

bool NavigateInAsh(const GURL& url) {
  // As requested by security, all additional queries will get removed.
  // Note that this will also be done on the Ash side for the same reason.
  const GURL& ash_url = crosapi::gurl_os_handler_utils::SanitizeAshURL(url);

  if (!IsUrlAcceptedByAsh(ash_url))
    return false;

  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::UrlHandler>())
    return false;

  service->GetRemote<crosapi::mojom::UrlHandler>()->OpenUrl(ash_url);
  return true;
}

}  // namespace lacros_url_handling
