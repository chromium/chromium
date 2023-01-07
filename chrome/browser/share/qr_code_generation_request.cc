// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/qr_code_generation_request.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/share/android/jni_headers/QRCodeGenerationRequest_jni.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

QRCodeGenerationRequest::QRCodeGenerationRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_data_string)
    : java_qr_code_generation_request_(j_caller) {
  std::string url_string(ConvertJavaStringToUTF8(env, j_data_string));

  qrcode_generator::mojom::GenerateQRCodeRequestPtr request =
      qrcode_generator::mojom::GenerateQRCodeRequest::New();
  request->data = url_string;
  request->should_render = true;
  request->center_image = qrcode_generator::mojom::CenterImage::CHROME_DINO;
  request->render_module_style = qrcode_generator::mojom::ModuleStyle::CIRCLES;
  request->render_locator_style =
      qrcode_generator::mojom::LocatorStyle::ROUNDED;

  remote_ = qrcode_generator::LaunchQRCodeGeneratorService();

  // On RPC error, we will still run the handler with a null bitmap.
  // The handler will call destroy() on this native object to clean up.
  // base::Unretained(this) is safe here because the callback won't be invoked
  // after |remote_| is destroyed, and |remote_| will be destroyed if this
  // object is destroyed.
  remote_.set_disconnect_handler(
      base::BindOnce(&QRCodeGenerationRequest::OnGenerateCodeResponse,
                     base::Unretained(this), nullptr));

  auto callback = base::BindOnce(
      &QRCodeGenerationRequest::OnGenerateCodeResponse, base::Unretained(this));
  remote_.get()->GenerateQRCode(std::move(request), std::move(callback));
}

QRCodeGenerationRequest::~QRCodeGenerationRequest() {}

void QRCodeGenerationRequest::Destroy(JNIEnv* env) {
  delete this;
}

void QRCodeGenerationRequest::OnGenerateCodeResponse(
    const qrcode_generator::mojom::GenerateQRCodeResponsePtr service_response) {
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!service_response ||
      service_response->error_code !=
          qrcode_generator::mojom::QRCodeGeneratorError::NONE) {
    Java_QRCodeGenerationRequest_onQRCodeAvailable(
        env, java_qr_code_generation_request_, nullptr);
    return;
  }

  // Convert the result to a Java Bitmap.
  ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(service_response->bitmap);
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
