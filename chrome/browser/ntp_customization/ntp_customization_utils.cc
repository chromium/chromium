// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "chrome/browser/ntp_customization/jni_headers/NtpCustomizationUtils_jni.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "skia/ext/image_operations.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/size.h"

namespace ntp_customization {

namespace {

// Maximum allowed image file size (25 MiB) to decode in the utility process.
constexpr uint64_t kMaxImageBytes = 25 * 1024 * 1024;
// Maximum pixel dimension (width/height) before downsampling is triggered.
constexpr int kMaxImageSize = 2556;

SkBitmap DownsampleImageIfNeededImpl(const SkBitmap& bitmap,
                                     int max_dimension) {
  int width = bitmap.width();
  int height = bitmap.height();
  if (width <= max_dimension && height <= max_dimension) {
    return bitmap;
  }

  int half_width = width / 2;
  int half_height = height / 2;
  int sample_size = 1;

  // Calculates the optimal power-of-2 sample size. Downsampling is only
  // performed if the halved dimensions are still larger than or equal to
  // max_dimension, ensuring slightly oversized images retain full sharpness.
  while ((half_width / sample_size) >= max_dimension ||
         (half_height / sample_size) >= max_dimension) {
    sample_size *= 2;
  }

  if (sample_size == 1) {
    return bitmap;
  }

  return skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BOX, width / sample_size,
      height / sample_size);
}

// Callback invoked when image decoding in the sandboxed Utility process
// finishes. Converts the decoded SkBitmap into an Android Bitmap and returns it
// to Java.
void OnImageDecoded(base::android::ScopedJavaGlobalRef<jobject> j_callback,
                    const SkBitmap& bitmap) {
  base::android::ScopedJavaLocalRef<jobject> j_bitmap;
  if (!bitmap.drawsNothing()) {
    SkBitmap scaled_bitmap = DownsampleImageIfNeededImpl(bitmap, kMaxImageSize);
    j_bitmap = gfx::ConvertToJavaBitmap(scaled_bitmap,
                                        gfx::OomBehavior::kReturnNullOnOom);
  }
  base::android::RunObjectCallbackAndroid(j_callback, j_bitmap);
}

}  // namespace

// Downsamples the bitmap if width or height exceeds max_dimension by
// halving dimensions to prevent Android Canvas/GPU texture rendering crashes.
SkBitmap DownsampleImageIfNeeded(const SkBitmap& bitmap, int max_dimension) {
  return DownsampleImageIfNeededImpl(bitmap, max_dimension);
}

// Decodes raw image bytes safely in an isolated utility sandbox process to
// prevent in-process memory corruption and satisfy Chromium's Rule of 2.
static void JNI_NtpCustomizationUtils_DecodeImage(
    JNIEnv* env,
    const std::vector<uint8_t>& data,
    const base::android::JavaRef<jobject>& j_callback) {
  data_decoder::DecodeImageIsolated(
      data, data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, kMaxImageBytes,
      // Desired frame size used by multi-frame container formats (e.g. .ico)
      // to select the closest matching resolution.
      gfx::Size(kMaxImageSize, kMaxImageSize),
      base::BindOnce(
          &OnImageDecoded,
          base::android::ScopedJavaGlobalRef<jobject>(env, j_callback)));
}

}  // namespace ntp_customization

DEFINE_JNI(NtpCustomizationUtils)
