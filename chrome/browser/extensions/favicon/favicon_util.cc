// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/favicon/favicon_util.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/favicon_size.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace extensions {
class Extension;

namespace favicon_util {

namespace {

int GetResourceID(int size_in_pixels) {
  bool is_dark = false;
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  int resource_id = is_dark ? IDR_DEFAULT_FAVICON : IDR_DEFAULT_FAVICON_DARK;
  is_dark = native_theme && native_theme->ShouldUseDarkColors();
  if (size_in_pixels >= 64) {
    resource_id =
        is_dark ? IDR_DEFAULT_FAVICON_DARK_64 : IDR_DEFAULT_FAVICON_64;
  } else if (size_in_pixels >= 32) {
    resource_id =
        is_dark ? IDR_DEFAULT_FAVICON_DARK_32 : IDR_DEFAULT_FAVICON_32;
  }
  return resource_id;
}

void OnFaviconAvailable(FaviconCallback callback,
                        int size_in_pixels,
                        const favicon_base::FaviconRawBitmapResult& result) {
  if (result.is_valid()) {
    std::move(callback).Run(result.bitmap_data);
  } else {
    std::move(callback).Run(
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
            GetResourceID(size_in_pixels),
            ui::GetSupportedResourceScaleFactor(1)));
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
  // pageUrl must be present.
  chrome::ParsedFaviconPath parsed;
  if (!ParseFaviconPath(url, &parsed) || parsed.page_url.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Use exact URL match instead of host match
  constexpr bool kAllowFallbackToHost = false;

  int size_in_pixels = parsed.size_in_dip;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon_service->GetRawFaviconForPageURL(
      GURL(parsed.page_url), {favicon_base::IconType::kFavicon}, size_in_pixels,
      kAllowFallbackToHost,
      base::BindOnce(&favicon_util::OnFaviconAvailable, std::move(callback),
                     size_in_pixels),
      tracker);
}

bool ParseFaviconPath(const GURL& url, chrome::ParsedFaviconPath* parsed) {
  if (!url.has_query()) {
    return false;
  }
  return chrome::ParseFaviconPath("?" + url.query(),
                                  chrome::FaviconUrlFormat::kFavicon2, parsed);
}

}  // namespace favicon_util
}  // namespace extensions
