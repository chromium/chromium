// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_IMAGE_FETCHER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_IMAGE_FETCHER_IMPL_H_

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "base/barrier_callback.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

class ProfileKey;

namespace autofill {

// chrome/ implementation of the AutofillImageFetcher. Used on Desktop (Android
// has its own Java implementation).
class AutofillImageFetcherImpl : public AutofillImageFetcher,
                                 public KeyedService {
 public:
  explicit AutofillImageFetcherImpl(ProfileKey* key);
  AutofillImageFetcherImpl(const AutofillImageFetcherImpl&) = delete;
  AutofillImageFetcherImpl& operator=(const AutofillImageFetcherImpl&) = delete;
  ~AutofillImageFetcherImpl() override;

  // AutofillImageFetcher:
  void FetchImagesForURLs(
      base::span<const GURL> card_art_urls,
      base::OnceCallback<void(const CardArtImageData&)> callback) override;

  // Called when an image is fetched for the `operation` instance.
  void OnCardArtImageFetched(
      base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
          barrier_callback,
      const GURL& card_art_url,
      const absl::optional<base::TimeTicks>& fetch_image_request_timestamp,
      const gfx::Image& card_art_image,
      const image_fetcher::RequestMetadata& metadata);

  // Returns the image with a grey overlay mask.
  static gfx::Image ApplyGreyOverlay(const gfx::Image& image);

 protected:
  // The image fetcher attached.
  raw_ptr<image_fetcher::ImageFetcher> image_fetcher_ = nullptr;

 private:
  // Helper function to fetch art image for card given the `card_art_url`.
  virtual void FetchImageForURL(
      base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
          barrier_callback,
      const GURL& card_art_url);

  void InitializeImageFetcher();

  raw_ptr<ProfileKey> key_;

  base::WeakPtrFactory<AutofillImageFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_IMAGE_FETCHER_IMPL_H_
