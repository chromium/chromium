// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_image_fetcher_impl.h"

#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/image_fetcher/core/cached_image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace autofill {

namespace {

constexpr char kUmaClientName[] = "AutofillImageFetcher";

constexpr net::NetworkTrafficAnnotationTag kCardArtImageTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("autofill_image_fetcher_card_art_image",
                                        R"(
      semantics {
        sender: "Autofill Image Fetcher"
        description:
          "Fetches customized card art images for credit cards stored in "
          "Chrome. Images are hosted on Google static content server, "
          "the data source may come from third parties (credit card issuers)."
        trigger: "When new credit card data is sent to Chrome if the card "
          "has a related card art image, and when the credit card data in "
          "the web database is refreshed and any card art image is missing."
        data: "URL of the image to be fetched."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can enable or disable this feature in Chromium settings by "
          "toggling 'Credit cards and addresses using Google Payments', "
          "under 'Advanced sync settings...'."
        chrome_policy {
          AutoFillEnabled {
            policy_options {mode: MANDATORY}
            AutoFillEnabled: false
          }
        }
      })");

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

void AutofillImageFetcherImpl::FetchImagesForURLs(
    base::span<const GURL> card_art_urls,
    base::OnceCallback<void(const CardArtImageData&)> callback) {
  InitializeImageFetcher();
  if (!image_fetcher_) {
    std::move(callback).Run({});
    return;
  }

  // Construct a BarrierCallback and so that the inner `callback` is invoked
  // only when all the images are fetched.
  const auto barrier_callback =
      base::BarrierCallback<std::unique_ptr<CreditCardArtImage>>(
          card_art_urls.size(), std::move(callback));

  for (const auto& card_art_url : card_art_urls)
    FetchImageForURL(barrier_callback, card_art_url);
}

void AutofillImageFetcherImpl::OnCardArtImageFetched(
    base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
        barrier_callback,
    const GURL& card_art_url,
    const absl::optional<base::TimeTicks>& fetch_image_request_timestamp,
    const gfx::Image& card_art_image,
    const image_fetcher::RequestMetadata& metadata) {
  // In case of an invalid url, `fetch_image_request_timestamp` is nullopt, and
  // hence we don't report any UMA metrics.
  if (fetch_image_request_timestamp.has_value()) {
    AutofillMetrics::LogImageFetcherRequestLatency(
        AutofillTickClock::NowTicks() - *fetch_image_request_timestamp);
  }
  AutofillMetrics::LogImageFetchResult(/*succeeded=*/!card_art_image.IsEmpty());

  auto credit_card_art_image =
      std::make_unique<CreditCardArtImage>(card_art_url, gfx::Image());
  if (!card_art_image.IsEmpty()) {
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableNewCardArtAndNetworkImages)) {
      if (card_art_url ==
          "https://www.gstatic.com/autofill/virtualcard/icon/"
          "capitalone_40_24.png") {
        // Render Capital One asset directly. No need to calculate and add grey
        // border to image.
        credit_card_art_image->card_art_image = card_art_image;
      } else {
        // Create the outer rectange. The outer rectangle is for the
        // entire image which includes the card art and additional border.
        gfx::RectF outer_rect =
            gfx::RectF(kCardArtImageWidth, kCardArtImageHeight);

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
                card_art_image.AsImageSkia(),
                skia::ImageOperations::RESIZE_BEST,
                gfx::Size(kCardArtImageWidth, kCardArtImageHeight)),
            outer_rect.x(), outer_rect.y(), card_art_paint);

        // Draw border around card art using outer rectangle.
        card_art_paint.setStrokeWidth(kCardArtBorderStrokeWidth);
        card_art_paint.setColor(kCardArtBorderColor);
        card_art_paint.setStyle(cc::PaintFlags::kStroke_Style);
        canvas.DrawRoundRect(outer_rect, kCardArtImageRadius, card_art_paint);

        // Add radius around entire image.
        credit_card_art_image->card_art_image =
            gfx::Image(gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
                kCardArtImageRadius,
                gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), 1.0f)));
      }
    } else {
      credit_card_art_image->card_art_image =
          AutofillImageFetcherImpl::ApplyGreyOverlay(card_art_image);
    }
  }

  std::move(barrier_callback).Run(std::move(credit_card_art_image));
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

void AutofillImageFetcherImpl::FetchImageForURL(
    base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
        barrier_callback,
    const GURL& card_art_url) {
  if (!card_art_url.is_valid()) {
    OnCardArtImageFetched(std::move(barrier_callback), card_art_url,
                          absl::nullopt, gfx::Image(),
                          image_fetcher::RequestMetadata());
    return;
  }

  GURL url;
  // TODO(crbug.com/1313616): There is only one gstatic card art image we are
  // using currently, that returns as metadata when it isn't. Remove this logic
  // and append FIFE URL suffix by default when the static image is deprecated,
  // and we send rich card art instead.
  // Check if the image is stored in Static Content Service. If not append the
  // FIFE URL option to fetch the correct image.
  if (card_art_url.spec() == kCapitalOneCardArtUrl) {
    url = base::FeatureList::IsEnabled(
              features::kAutofillEnableNewCardArtAndNetworkImages)
              ? GURL(kCapitalOneLargeCardArtUrl)
              : card_art_url;
  } else {
    // A FIFE image fetching param suffix is appended to the URL. The image
    // should be center cropped and of Size(32, 20) unless the
    // kAutofillEnableNewCardArtAndNetworkImages feature is enabled, in which
    // case we take the image at its raw size and resize it to Size(40, 24)
    // later.
    url = base::FeatureList::IsEnabled(
              features::kAutofillEnableNewCardArtAndNetworkImages)
              ? card_art_url
              : GURL(card_art_url.spec() + "=w32-h20-n");
  }

  image_fetcher::ImageFetcherParams params(kCardArtImageTrafficAnnotation,
                                           kUmaClientName);
  image_fetcher_->FetchImage(
      url,
      base::BindOnce(&AutofillImageFetcherImpl::OnCardArtImageFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(barrier_callback), card_art_url,
                     AutofillTickClock::NowTicks()),
      std::move(params));
}

void AutofillImageFetcherImpl::InitializeImageFetcher() {
  if (image_fetcher_)
    return;

  // Lazily initialize the `image_fetcher_`, since
  // ImageFetcherServiceFactory relies on the initialization of the profile, and
  // when AutofillImageFetcher is created, the initialization of the profile is
  // not done yet.
  image_fetcher::ImageFetcherService* image_fetcher_service =
      ImageFetcherServiceFactory::GetForKey(key_);
  if (!image_fetcher_service)
    return;

  // TODO(crbug.com/1382289): Fix and change the config back to kDiskCacheOnly.
  image_fetcher_ = image_fetcher_service->GetImageFetcher(
      image_fetcher::ImageFetcherConfig::kNetworkOnly);
}

}  // namespace autofill
