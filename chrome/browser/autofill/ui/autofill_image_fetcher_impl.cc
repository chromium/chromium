// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/ui/autofill_image_fetcher_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// The image radius value for card art images.
constexpr int kCardArtImageRadius = 3;  // 3dp

// The SkAlpha value for the image grey overlay.
constexpr double kImageOverlayAlpha = 0.04;  // 4%

// The border color used for card art images.
constexpr SkColor kCardArtBorderColor = SkColorSetARGB(0xFF, 0xE3, 0xE3, 0xE3);

// The stroke width of the card art border.
constexpr int kCardArtBorderStrokeWidth = 2;

// The width and length card art is resized to.
constexpr int kCardArtImageWidth = 48;
constexpr int kCardArtImageHeight = 30;

int CardArtImageWidth() {
  return kCardArtImageWidth;
}

int CardArtImageHeight() {
  return kCardArtImageHeight;
}

}  // namespace

AutofillImageFetcherImpl::AutofillImageFetcherImpl(ProfileKey* key)
    : key_(key) {}

AutofillImageFetcherImpl::~AutofillImageFetcherImpl() = default;

image_fetcher::ImageFetcher* AutofillImageFetcherImpl::GetImageFetcher() {
  InitializeImageFetcher();
  return image_fetcher_;
}

base::WeakPtr<AutofillImageFetcher> AutofillImageFetcherImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

GURL AutofillImageFetcherImpl::ResolveImageURL(const GURL& image_url,
                                               ImageType image_type) const {
  switch (image_type) {
    case ImageType::kCreditCardArtImage: {
      // TODO(crbug.com/40221039): There is only one gstatic card art image we
      // are using currently, that returns as metadata when it isn't. Remove
      // this logic when the static image is deprecated, and we send rich card
      // art instead.
      if (image_url.spec() == kCapitalOneCardArtUrl) {
        return GURL(kCapitalOneLargeCardArtUrl);
      }

      // We take the image at height 48*scale and its ratio width, then later
      // resize it to Size(40, 24).
      ui::ResourceScaleFactor scale_factor = ui::k100Percent;
      // Use the scaling that matches primary display.
      if (display::Screen* screen = display::Screen::Get()) {
        scale_factor = ui::GetSupportedResourceScaleFactor(
            screen->GetPrimaryDisplay().device_scale_factor());
      }
      // Calculate the height as an integer to avoid floating point formatting
      // issues in the URL.
      int height = base::ClampRound(
          48 * ui::GetScaleForResourceScaleFactor(scale_factor));
      return GURL(base::StrCat(
          {image_url.spec(), "=h", base::NumberToString(height), "-pa"}));
    }
    case ImageType::kPixAccountImage:
      // Pay with Pix is only queried in Chrome on Android.
      NOTREACHED();
    case ImageType::kValuableImage:
      return GURL(image_url.spec() + "=h96-w96-cc-rp");
  }
}

gfx::Image AutofillImageFetcherImpl::ResolveCardArtImage(
    const GURL& card_art_url,
    const gfx::Image& card_art_image) {
  if (card_art_url == kCapitalOneLargeCardArtUrl) {
    // Render Capital One asset directly. No need to calculate and add grey
    // border to image.
    return card_art_image;
  }

  float scale = 1.0f;
  if (display::Screen* screen = display::Screen::Get()) {
    scale = screen->GetPrimaryDisplay().device_scale_factor();
  }

  // Create the outer rectangle. The outer rectangle is for the
  // entire image which includes the card art and additional border.
  gfx::RectF outer_rect = gfx::RectF(CardArtImageWidth(), CardArtImageHeight());

  // The inner rectangle only includes the card art. To calculate the
  // inner rectangle, we need to factor the space that the border stroke
  // will take up.
  gfx::RectF inner_rect = gfx::RectF(
      /*x=*/kCardArtBorderStrokeWidth, /*y=*/kCardArtBorderStrokeWidth,
      /*width=*/CardArtImageWidth() - (kCardArtBorderStrokeWidth * 2),
      /*height=*/CardArtImageHeight() - (kCardArtBorderStrokeWidth * 2));
  gfx::Canvas canvas =
      gfx::Canvas(gfx::Size(CardArtImageWidth(), CardArtImageHeight()),
                  /*image_scale=*/scale, /*is_opaque=*/false);
  cc::PaintFlags card_art_paint;
  card_art_paint.setAntiAlias(true);

  // Draw card art with rounded corners in the inner rectangle.
  canvas.DrawRoundRect(inner_rect, kCardArtImageRadius, card_art_paint);
  gfx::ImageSkia card_art_image_skia = card_art_image.AsImageSkia();
  canvas.DrawImageInt(card_art_image_skia, /*src_x=*/0, /*src_y=*/0,
                      card_art_image_skia.width(), card_art_image_skia.height(),
                      outer_rect.x(), outer_rect.y(), outer_rect.width(),
                      outer_rect.height(),
                      /*filter=*/true, card_art_paint);

  // Draw border around card art using outer rectangle.
  card_art_paint.setStrokeWidth(kCardArtBorderStrokeWidth);
  card_art_paint.setColor(kCardArtBorderColor);
  card_art_paint.setStyle(cc::PaintFlags::kStroke_Style);
  canvas.DrawRoundRect(outer_rect, kCardArtImageRadius, card_art_paint);

  // Add radius around entire image.
  return gfx::Image(gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
      kCardArtImageRadius,
      gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), scale)));
}

gfx::Image AutofillImageFetcherImpl::ResolveValuableImage(
    const gfx::Image& valuable_image) {
  // Increase image scale from 1.0 to 4.0 to render higher quality images on
  // high resolution displays. This decreases the image size from 96x96 to
  // 24x24.
  return gfx::Image(
      gfx::ImageSkia::CreateFromBitmap(valuable_image.AsBitmap(), 4.0f));
}

// static
gfx::Image AutofillImageFetcherImpl::ApplyGreyOverlay(const gfx::Image& image) {
  // Create a solid dark grey mask for the image.
  gfx::ImageSkia mask = gfx::ImageSkiaOperations::CreateColorMask(
      image.AsImageSkia(), SK_ColorDKGRAY);
  // Apply the mask to the original card art image with alpha set to
  // `kImageOverlayAlpha`.
  return gfx::Image(gfx::ImageSkiaOperations::CreateBlendedImage(
      image.AsImageSkia(), mask, kImageOverlayAlpha));
}

void AutofillImageFetcherImpl::InitializeImageFetcher() {
  if (image_fetcher_) {
    return;
  }

  // Lazily initialize the `image_fetcher_`, since
  // ImageFetcherServiceFactory relies on the initialization of the profile, and
  // when AutofillImageFetcher is created, the initialization of the profile is
  // not done yet.
  image_fetcher::ImageFetcherService* image_fetcher_service =
      ImageFetcherServiceFactory::GetForKey(key_);
  if (!image_fetcher_service) {
    return;
  }

  image_fetcher_ = image_fetcher_service->GetImageFetcher(
      image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
}

}  // namespace autofill
