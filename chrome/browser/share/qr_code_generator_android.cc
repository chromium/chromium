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
#include "chrome/android/chrome_jni_headers/QRCodeGenerator_jni.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject> JNI_QRCodeGenerator_GenerateBitmap(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_data_string) {
  std::string url_string(ConvertJavaStringToUTF8(env, j_data_string));
  auto qr_image = qr_code_generator::GenerateBitmap(
      base::as_byte_span(url_string), qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kDino);

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (qr_image.has_value()) {
    java_bitmap = gfx::ConvertToJavaBitmap(qr_image->bitmap);
  }
  return java_bitmap;
}
