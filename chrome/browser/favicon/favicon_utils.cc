// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_utils.h"

#include "base/hash/sha1.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/monogram_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace favicon {

namespace {

// The color of the letter drawn for a fallback icon.  Changing this may require
// changing the algorithm in ReturnRenderedIconForRequest() that guarantees
// contrast.
constexpr SkColor kFallbackIconLetterColor = SK_ColorWHITE;

// Desaturate favicon HSL shift values.
const double kDesaturateHue = -1.0;
const double kDesaturateSaturation = 0.0;
const double kDesaturateLightness = 0.6;

// Returns a color based on the hash of |icon_url|'s origin.
SkColor ComputeBackgroundColorForUrl(const GURL& icon_url) {
  if (!icon_url.is_valid())
    return SK_ColorGRAY;

  base::SHA1Digest hash = base::SHA1Hash(
      base::as_byte_span(icon_url.DeprecatedGetOriginAsURL().spec()));
  return SkColorSetRGB(hash[0u], hash[1u], hash[2u]);
}

// Gets the appropriate light or dark rasterized default favicon.
gfx::Image GetDefaultFaviconForColorScheme(bool is_dark) {
  const int resource_id =
      is_dark ? IDR_DEFAULT_FAVICON_DARK : IDR_DEFAULT_FAVICON;
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace

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

SkBitmap GenerateMonogramFavicon(GURL url, int icon_size, int circle_size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(icon_size, icon_size, false);
  cc::SkiaPaintCanvas paint_canvas(bitmap);
  gfx::Canvas canvas(&paint_canvas, 1.f);

  std::u16string monogram = favicon::GetFallbackIconText(url);
  SkColor fallback_color =
      color_utils::BlendForMinContrast(ComputeBackgroundColorForUrl(url),
                                       kFallbackIconLetterColor)
          .color;

  monogram::DrawMonogramInCanvas(&canvas, icon_size, circle_size, monogram,
                                 kFallbackIconLetterColor, fallback_color);
  return bitmap;
}

gfx::Image TabFaviconFromWebContents(content::WebContents* contents) {
  DCHECK(contents);

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(contents);
  // TODO(crbug.com/40190724): Investigate why some WebContents do not have
  // an attached ContentFaviconDriver.
  if (!favicon_driver) {
    return gfx::Image();
  }

  gfx::Image favicon = favicon_driver->GetFavicon();

  // Desaturate the favicon if the navigation entry contains a network error.
  if (!contents->ShouldShowLoadingUI()) {
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
  bool is_dark = false;
#if !BUILDFLAG(IS_ANDROID)
  // Android doesn't currently implement NativeTheme::GetInstanceForNativeUi.
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  is_dark = native_theme && native_theme->ShouldUseDarkColors();
#endif
  return GetDefaultFaviconForColorScheme(is_dark);
}

ui::ImageModel GetDefaultFaviconModel(ui::ColorId bg_color) {
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(
          [](ui::ColorId bg_color, const ui::ColorProvider* provider) {
            return *GetDefaultFaviconForColorScheme(
                        color_utils::IsDark(provider->GetColor(bg_color)))
                        .ToImageSkia();
          },
          bg_color),
      gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));
}

void SaveFaviconEvenIfInIncognito(content::WebContents* contents) {
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  Profile* original_profile =
      Profile::FromBrowserContext(contents->GetBrowserContext())
          ->GetOriginalProfile();
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(original_profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  // Make sure the page is in history, otherwise adding the favicon does
  // nothing.
  GURL page_url = entry->GetURL();
  favicon_service->AddPageNoVisitForBookmark(page_url, entry->GetTitle());

  const content::FaviconStatus& favicon_status = entry->GetFavicon();
  if (!favicon_status.valid || favicon_status.url.is_empty() ||
      favicon_status.image.IsEmpty()) {
    return;
  }

  favicon_service->SetFavicons({page_url}, favicon_status.url,
                               favicon_base::IconType::kFavicon,
                               favicon_status.image);
}

bool ShouldThemifyFavicon(GURL url) {
  if (!url.SchemeIs(content::kChromeUIScheme)) {
    return false;
  }
  return url.host_piece() != chrome::kChromeUIAppLauncherPageHost &&
         url.host_piece() != chrome::kChromeUIHelpHost &&
         url.host_piece() != chrome::kChromeUIVersionHost &&
         url.host_piece() != chrome::kChromeUINetExportHost &&
         url.host_piece() != chrome::kChromeUINewTabHost &&
         url.host_piece() != password_manager::kChromeUIPasswordManagerHost;
}

bool ShouldThemifyFaviconForEntry(content::NavigationEntry* entry) {
  const GURL& virtual_url = entry->GetVirtualURL();
  const GURL& actual_url = entry->GetURL();

  if (ShouldThemifyFavicon(virtual_url)) {
    return true;
  }

  // Themify favicon for the default NTP and incognito NTP.
  if (actual_url.SchemeIs(content::kChromeUIScheme)) {
    return actual_url.host_piece() == chrome::kChromeUINewTabPageHost ||
           actual_url.host_piece() == chrome::kChromeUINewTabHost;
  }

  return false;
}

gfx::ImageSkia ThemeFavicon(const gfx::ImageSkia& source,
                            SkColor alternate_color,
                            SkColor active_background,
                            SkColor inactive_background) {
  // Choose between leaving the image as-is or masking with |alternate_color|.
  const SkColor original_color =
      color_utils::CalculateKMeanColorOfBitmap(*source.bitmap());

  // Compute the minimum contrast of each color against active and inactive
  // backgrounds.
  const float original_contrast = std::min(
      color_utils::GetContrastRatio(original_color, active_background),
      color_utils::GetContrastRatio(original_color, inactive_background));
  const float alternate_contrast = std::min(
      color_utils::GetContrastRatio(alternate_color, active_background),
      color_utils::GetContrastRatio(alternate_color, inactive_background));

  // Recolor the image if the original has low minimum contrast and recoloring
  // will improve it.
  return ((original_contrast < color_utils::kMinimumVisibleContrastRatio) &&
          (alternate_contrast > original_contrast))
             ? gfx::ImageSkiaOperations::CreateColorMask(source,
                                                         alternate_color)
             : source;
}

gfx::ImageSkia ThemeMonochromeFavicon(const gfx::ImageSkia& source,
                                      SkColor background) {
  return (color_utils::GetContrastRatio(gfx::kGoogleGrey900, background) >
          color_utils::GetContrastRatio(SK_ColorWHITE, background))
             ? gfx::ImageSkiaOperations::CreateColorMask(source,
                                                         gfx::kGoogleGrey900)
             : gfx::ImageSkiaOperations::CreateColorMask(source, SK_ColorWHITE);
}

}  // namespace favicon
