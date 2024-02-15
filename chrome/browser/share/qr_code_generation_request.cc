// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/qr_code_generation_request.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/QRCodeGenerationRequest_jni.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

QRCodeGenerationRequest::~QRCodeGenerationRequest() {}

void QRCodeGenerationRequest::Destroy(JNIEnv* env) {
  delete this;
}

QRCodeGenerationRequest::QRCodeGenerationRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_data_string)
    : java_qr_code_generation_request_(j_caller) {
  std::string url_string(ConvertJavaStringToUTF8(env, j_data_string));
  auto qr_image = qr_code_generator::GenerateBitmap(
      base::as_byte_span(url_string), qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kDino);

  if (!qr_image.has_value()) {
    Java_QRCodeGenerationRequest_onQRCodeAvailable(
        env, java_qr_code_generation_request_, nullptr);
    return;
  }

  // Convert the result to a Java Bitmap.
  ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(qr_image->bitmap);
  Java_QRCodeGenerationRequest_onQRCodeAvailable(
      env, java_qr_code_generation_request_, java_bitmap);
}

static jlong JNI_QRCodeGenerationRequest_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_data_string) {
  return reinterpret_cast<intptr_t>(
      new QRCodeGenerationRequest(env, j_caller, j_data_string));
}
