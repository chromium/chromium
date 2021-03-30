// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_utils.h"

#include "base/hash/sha1.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/favicon_service.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"

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

// Draws a circle of a given |size| and |offset| in the |canvas| and fills it
// with |background_color|.
void DrawCircleInCanvas(gfx::Canvas* canvas,
                        int size,
                        int offset,
                        SkColor background_color) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(background_color);
  int corner_radius = size / 2;
  canvas->DrawRoundRect(gfx::Rect(offset, offset, size, size), corner_radius,
                        flags);
}

// Will paint the appropriate letter in the center of specified |canvas| of
// given |size|.
void DrawFallbackIconLetter(const GURL& icon_url,
                            int size,
                            int offset,
                            gfx::Canvas* canvas) {
  // Get the appropriate letter to draw, then eventually draw it.
  std::u16string icon_text = favicon::GetFallbackIconText(icon_url);
  if (icon_text.empty())
    return;

  const double kDefaultFontSizeRatio = 0.5;
  int font_size = static_cast<int>(size * kDefaultFontSizeRatio);
  if (font_size <= 0)
    return;

  gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL;

#if defined(OS_WIN)
  font_weight = gfx::Font::Weight::SEMIBOLD;
#endif

  // TODO(crbug.com/853780): Adjust the text color according to the background
  // color.
  canvas->DrawStringRectWithFlags(
      icon_text,
      gfx::FontList({l10n_util::GetStringUTF8(IDS_NTP_FONT_FAMILY)},
                    gfx::Font::NORMAL, font_size, font_weight),
      kFallbackIconLetterColor, gfx::Rect(offset, offset, size, size),
      gfx::Canvas::TEXT_ALIGN_CENTER);
}

// Returns a color based on the hash of |icon_url|'s origin.
SkColor ComputeBackgroundColorForUrl(const GURL& icon_url) {
  if (!icon_url.is_valid())
    return SK_ColorGRAY;

  unsigned char hash[20];
  const std::string origin = icon_url.GetOrigin().spec();
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(origin.c_str()),
                      origin.size(), hash);
  return SkColorSetRGB(hash[0], hash[1], hash[2]);
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
  canvas.DrawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
  SkColor fallback_color =
      color_utils::BlendForMinContrast(ComputeBackgroundColorForUrl(url),
                                       kFallbackIconLetterColor)
          .color;

  int offset = (icon_size - circle_size) / 2;
  DrawCircleInCanvas(&canvas, circle_size, offset, fallback_color);
  DrawFallbackIconLetter(url, circle_size, offset, &canvas);
  return bitmap;
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

}  // namespace favicon
