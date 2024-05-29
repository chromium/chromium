// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/QRCodeGenerator_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject> JNI_QRCodeGenerator_GenerateBitmap(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_data_string) {
  // TODO(https://crbug.com/325664342): Audit if `QuietZone::kIncluded`
  // can/should be used instead (this may require testing if the different image
  // size works well with surrounding UI elements).  Note that the absence of a
  // quiet zone may interfere with decoding of QR codes even for small codes
  // (for examples see #comment8, #comment9 and #comment6 in the bug).
  std::string url_string(ConvertJavaStringToUTF8(env, j_data_string));
  auto qr_image = qr_code_generator::GenerateBitmap(
      base::as_byte_span(url_string), qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kDino,
      qr_code_generator::QuietZone::kWillBeAddedByClient);

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (qr_image.has_value()) {
    java_bitmap = gfx::ConvertToJavaBitmap(qr_image.value());
  }
  return java_bitmap;
}
