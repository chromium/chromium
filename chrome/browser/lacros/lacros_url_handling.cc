// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_url_handling.h"

#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "url/gurl.h"

namespace lacros_url_handling {

bool MaybeInterceptNavigation(const GURL& url) {
  // For now, just intercept the os-settings URL.
  if (url.DeprecatedGetOriginAsURL() !=
      GURL(chrome::kChromeUIOSSettingsURL).DeprecatedGetOriginAsURL())
    return false;

  // We may expand this in the future to support a dynamic set of URLs provided
  // by Ash via LacrosInitParams. That way we avoid having to synchronize the
  // set of known chrome:// URLs across the two sides.
  return NavigateInAsh(url);
}

bool NavigateInAsh(const GURL& url) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::UrlHandler>())
    return false;

  service->GetRemote<crosapi::mojom::UrlHandler>()->OpenUrl(url);
  return true;
}

}  // namespace lacros_url_handling
