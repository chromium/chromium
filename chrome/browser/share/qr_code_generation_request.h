// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_QR_CODE_GENERATION_REQUEST_H_
#define CHROME_BROWSER_SHARE_QR_CODE_GENERATION_REQUEST_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "ui/gfx/android/java_bitmap.h"

// A wrapper class exposing the QR Code Mojo service to Java.
class QRCodeGenerationRequest {
 public:
  QRCodeGenerationRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_caller,
      const base::android::JavaParamRef<jstring>& j_data_string);
  void Destroy(JNIEnv* env);

  QRCodeGenerationRequest(const QRCodeGenerationRequest&) = delete;
  QRCodeGenerationRequest& operator=(const QRCodeGenerationRequest&) = delete;

 private:
  virtual ~QRCodeGenerationRequest();

  void OnGenerateCodeResponse(
      const qrcode_generator::mojom::GenerateQRCodeResponsePtr
          service_response);

  // Reference to Java QRCodeGenerationRequest containing a callback method.
  base::android::ScopedJavaGlobalRef<jobject> java_qr_code_generation_request_;

  // TODO(https://crbug.com/1431991): Remove this field once there is no
  // internal state (e.g. no `mojo::Remote`) that needs to be maintained by the
  // `QRImageGenerator` class.
  std::unique_ptr<qrcode_generator::QRImageGenerator> qrcode_service_;
};

#endif  // CHROME_BROWSER_SHARE_QR_CODE_GENERATION_REQUEST_H_
