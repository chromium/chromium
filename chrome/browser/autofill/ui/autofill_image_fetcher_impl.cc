// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/ui/autofill_image_fetcher_impl.h"

#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "third_party/skia/include/core/SkColor.h"
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
constexpr int kCardArtImageWidth = 40;
constexpr int kCardArtImageHeight = 24;

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

GURL AutofillImageFetcherImpl::ResolveCardArtURL(const GURL& card_art_url) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableNewCardArtAndNetworkImages)) {
    return AutofillImageFetcher::ResolveCardArtURL(card_art_url);
  }

  // TODO(crbug.com/40221039): There is only one gstatic card art image we are
  // using currently, that returns as metadata when it isn't. Remove this logic
  // when the static image is deprecated, and we send rich card art instead.
  if (card_art_url.spec() == kCapitalOneCardArtUrl) {
    return GURL(kCapitalOneLargeCardArtUrl);
  }

  // When kAutofillEnableNewCardArtAndNetworkImages is enabled, we take the
  // image at height 48 with its ratio width and resize to Size(40, 24) later
  return GURL(card_art_url.spec() + "=h48-pa");
}

gfx::Image AutofillImageFetcherImpl::ResolveCardArtImage(
    const GURL& card_art_url,
    const gfx::Image& card_art_image) {
  if (card_art_image.IsEmpty()) {
    return card_art_image;
  }

  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableNewCardArtAndNetworkImages)) {
    return AutofillImageFetcherImpl::ApplyGreyOverlay(card_art_image);
  }

  if (card_art_url == kCapitalOneLargeCardArtUrl) {
    // Render Capital One asset directly. No need to calculate and add grey
    // border to image.
    return card_art_image;
  }

  // Create the outer rectangle. The outer rectangle is for the
  // entire image which includes the card art and additional border.
  gfx::RectF outer_rect = gfx::RectF(kCardArtImageWidth, kCardArtImageHeight);

  // The inner rectangle only includes the card art. To calculate the
  // inner rectangle, we need to factor the space that the border stroke
  // will take up.
  gfx::RectF inner_rect = gfx::RectF(
      /*x=*/kCardArtBorderStrokeWidth, /*y=*/kCardArtBorderStrokeWidth,
      /*width=*/kCardArtImageWidth - (kCardArtBorderStrokeWidth * 2),
      /*height=*/kCardArtImageHeight - (kCardArtBorderStrokeWidth * 2));
  gfx::Canvas canvas =
      gfx::Canvas(gfx::Size(kCardArtImageWidth, kCardArtImageHeight),
                  /*image_scale=*/1.0f, /*is_opaque=*/false);
  cc::PaintFlags card_art_paint;
  card_art_paint.setAntiAlias(true);

  // Draw card art with rounded corners in the inner rectangle.
  canvas.DrawRoundRect(inner_rect, kCardArtImageRadius, card_art_paint);
  canvas.DrawImageInt(
      gfx::ImageSkiaOperations::CreateResizedImage(
          card_art_image.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kCardArtImageWidth, kCardArtImageHeight)),
      outer_rect.x(), outer_rect.y(), card_art_paint);

  // Draw border around card art using outer rectangle.
  card_art_paint.setStrokeWidth(kCardArtBorderStrokeWidth);
  card_art_paint.setColor(kCardArtBorderColor);
  card_art_paint.setStyle(cc::PaintFlags::kStroke_Style);
  canvas.DrawRoundRect(outer_rect, kCardArtImageRadius, card_art_paint);

  // Add radius around entire image.
  return gfx::Image(gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
      kCardArtImageRadius,
      gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), 1.0f)));
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

  // TODO(crbug.com/40245547): Fix and change the config back to kDiskCacheOnly.
  image_fetcher_ = image_fetcher_service->GetImageFetcher(
      image_fetcher::ImageFetcherConfig::kNetworkOnly);
}

}  // namespace autofill
