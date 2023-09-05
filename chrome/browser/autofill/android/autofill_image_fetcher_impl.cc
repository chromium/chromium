// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_image_fetcher_impl.h"

#include "base/android/jni_android.h"
#include "url/android/gurl_android.h"

namespace autofill {

AutofillImageFetcherImpl::AutofillImageFetcherImpl(ProfileKey* key)
    : key_(key) {}

AutofillImageFetcherImpl::~AutofillImageFetcherImpl() = default;

void AutofillImageFetcherImpl::FetchImagesForURLs(
    base::span<const GURL> card_art_urls,
    base::OnceCallback<void(
        const std::vector<std::unique_ptr<CreditCardArtImage>>&)> callback) {
  // TODO (crbug.com/1467754): Implement images prefetching for Android.
}

}  // namespace autofill
