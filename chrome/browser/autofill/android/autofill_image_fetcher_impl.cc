// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_image_fetcher_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile_key_android.h"
#include "ui/gfx/image/image.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/AutofillImageFetcher_jni.h"

namespace autofill {

AutofillImageFetcherImpl::AutofillImageFetcherImpl(ProfileKey* key)
    : key_(key) {}

AutofillImageFetcherImpl::~AutofillImageFetcherImpl() = default;

void AutofillImageFetcherImpl::FetchCreditCardArtImagesForURLs(
    base::span<const GURL> image_urls,
    base::span<const AutofillImageFetcherBase::ImageSize> image_sizes) {
  if (image_urls.empty()) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  // TODO(crbug.com/388217006): Move all image fetch calls to the new API.
  std::vector<int> image_sizes_vector;
  std::transform(image_sizes.begin(), image_sizes.end(),
                 std::back_inserter(image_sizes_vector),
                 [](auto image_size) { return static_cast<int>(image_size); });
  Java_AutofillImageFetcher_prefetchCardArtImages(
      env, GetOrCreateJavaImageFetcher(), image_urls,
      base::android::ToJavaIntArray(env, image_sizes_vector));
}

void AutofillImageFetcherImpl::FetchPixAccountImagesForURLs(
    base::span<const GURL> image_urls) {
  if (image_urls.empty()) {
    return;
  }
  Java_AutofillImageFetcher_prefetchPixAccountImages(
      base::android::AttachCurrentThread(), GetOrCreateJavaImageFetcher(),
      image_urls);
}

void AutofillImageFetcherImpl::FetchValuableImagesForURLs(
    base::span<const GURL> image_urls) {
  if (image_urls.empty()) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillImageFetcher_prefetchValuableImages(
      env, GetOrCreateJavaImageFetcher(), image_urls);
}

const gfx::Image* AutofillImageFetcherImpl::GetCachedImageForUrl(
    const GURL& image_url,
    ImageType image_type) const {
  // The images are cached on the Java side on Android.
  return nullptr;
}

base::android::ScopedJavaLocalRef<jobject>
AutofillImageFetcherImpl::GetOrCreateJavaImageFetcher() {
  if (!java_image_fetcher_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_image_fetcher_ = Java_AutofillImageFetcher_create(
        env, key_->GetProfileKeyAndroid()->GetJavaObject());
  }

  return base::android::ScopedJavaLocalRef<jobject>(java_image_fetcher_);
}

}  // namespace autofill
