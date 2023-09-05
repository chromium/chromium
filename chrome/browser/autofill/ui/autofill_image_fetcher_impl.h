// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_UI_AUTOFILL_IMAGE_FETCHER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_UI_AUTOFILL_IMAGE_FETCHER_IMPL_H_

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"

#include "components/keyed_service/core/keyed_service.h"

class ProfileKey;

namespace autofill {

// chrome/ implementation of AutofillImageFetcher for Desktop clients.
class AutofillImageFetcherImpl : public AutofillImageFetcher,
                                 public KeyedService {
 public:
  explicit AutofillImageFetcherImpl(ProfileKey* key);
  AutofillImageFetcherImpl(const AutofillImageFetcherImpl&) = delete;
  AutofillImageFetcherImpl& operator=(const AutofillImageFetcherImpl&) = delete;
  ~AutofillImageFetcherImpl() override;

  // AutofillImageFetcher:
  image_fetcher::ImageFetcher* GetImageFetcher() override;
  base::WeakPtr<AutofillImageFetcher> GetWeakPtr() override;
  GURL ResolveCardArtURL(const GURL& card_art_url) override;
  gfx::Image ResolveCardArtImage(const GURL& card_art_url,
                                 const gfx::Image& card_art_image) override;

  // Returns the image with a grey overlay mask.
  static gfx::Image ApplyGreyOverlay(const gfx::Image& image);

 protected:
  // The image fetcher attached.
  raw_ptr<image_fetcher::ImageFetcher> image_fetcher_ = nullptr;

 private:
  void InitializeImageFetcher();

  raw_ptr<ProfileKey> key_;

  base::WeakPtrFactory<AutofillImageFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_UI_AUTOFILL_IMAGE_FETCHER_IMPL_H_
