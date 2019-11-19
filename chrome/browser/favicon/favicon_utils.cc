// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_utils.h"

#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/url_constants.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/favicon_url.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"

namespace favicon {

namespace {

// Desaturate favicon HSL shift values.
const double kDesaturateHue = -1.0;
const double kDesaturateSaturation = 0.0;
const double kDesaturateLightness = 0.6;
}

void CreateContentFaviconDriverForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (ContentFaviconDriver::FromWebContents(web_contents))
    return;

  Profile* original_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetOriginalProfile();
  return ContentFaviconDriver::CreateForWebContents(
      web_contents,
      FaviconServiceFactory::GetForProfile(original_profile,
                                           ServiceAccessType::IMPLICIT_ACCESS));
}

bool ShouldDisplayFavicon(content::WebContents* web_contents) {
  // No favicon on interstitials.
  if (web_contents->ShowingInterstitialPage())
    return false;

  // Suppress the icon for the new-tab page, even if a navigation to it is
  // not committed yet. Note that we're looking at the visible URL, so
  // navigations from NTP generally don't hit this case and still show an icon.
  GURL url = web_contents->GetVisibleURL();
  if (url.SchemeIs(content::kChromeUIScheme) &&
      url.host_piece() == chrome::kChromeUINewTabHost) {
    return false;
  }

  // Also suppress instant-NTP. This does not use search::IsInstantNTP since
  // it looks at the last-committed entry and we need to show icons for pending
  // navigations away from it.
  if (search::IsInstantNTPURL(url, Profile::FromBrowserContext(
                                       web_contents->GetBrowserContext()))) {
    return false;
  }

  // Otherwise, always display the favicon.
  return true;
}

gfx::Image TabFaviconFromWebContents(content::WebContents* contents) {
  DCHECK(contents);

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(contents);
  gfx::Image favicon = favicon_driver->GetFavicon();

  // Desaturate the favicon if the navigation entry contains a network error.
  if (!contents->IsLoadingToDifferentDocument()) {
    content::NavigationController& controller = contents->GetController();

    content::NavigationEntry* entry = controller.GetLastCommittedEntry();
    if (entry && (entry->GetPageType() == content::PAGE_TYPE_ERROR)) {
      color_utils::HSL shift = {kDesaturateHue, kDesaturateSaturation,
                                kDesaturateLightness};
      return gfx::Image(gfx::ImageSkiaOperations::CreateHSLShiftedImage(
          *favicon.ToImageSkia(), shift));
    }
  }

  return favicon;
}

gfx::Image GetDefaultFavicon() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  bool is_dark = native_theme && native_theme->ShouldUseDarkColors();
  int resource_id = is_dark ? IDR_DEFAULT_FAVICON_DARK : IDR_DEFAULT_FAVICON;
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace favicon
