// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/favicon/favicon_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace extensions {
class Extension;

namespace favicon_util {

namespace {
void OnFaviconAvailable(FaviconCallback callback,
                        const favicon_base::FaviconRawBitmapResult& result) {
  if (result.is_valid()) {
    std::move(callback).Run(result.bitmap_data);
  } else {
    // TODO(solomonkinard): Use higher-res defaults for various sizes.
    std::move(callback).Run(
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
            IDR_DEFAULT_FAVICON, ui::GetSupportedResourceScaleFactor(1)));
  }
}
}  // namespace

void GetFaviconForExtensionRequest(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   const GURL& url,
                                   base::CancelableTaskTracker* tracker,
                                   FaviconCallback callback) {
  // Validation.
  if (!extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kFavicon) ||
      extension->manifest_version() < 3) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Parse url. Restrict which parameters are exposed to the Extension API.
  chrome::ParsedFaviconPath parsed;
  if (!ParseFaviconPath(url, &parsed)) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Don't make requests to the favicon server if a favicon can't be found.
  constexpr bool kAllowFaviconServerFallback = false;

  int size_in_pixels = parsed.size_in_dip;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon_service->GetRawFaviconForPageURL(
      GURL(parsed.page_url), {favicon_base::IconType::kFavicon}, size_in_pixels,
      kAllowFaviconServerFallback,
      base::BindOnce(&favicon_util::OnFaviconAvailable, std::move(callback)),
      tracker);
}

bool ParseFaviconPath(const GURL& url, chrome::ParsedFaviconPath* parsed) {
  if (!url.has_query())
    return false;
  return chrome::ParseFaviconPath("?" + url.query(),
                                  chrome::FaviconUrlFormat::kFavicon2, parsed);
}

}  // namespace favicon_util
}  // namespace extensions
