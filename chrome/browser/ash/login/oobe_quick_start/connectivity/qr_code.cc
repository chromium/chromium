// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"

#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "url/url_util.h"

namespace ash::quick_start {

namespace {

// The target device's device type. 7 = CHROME. Values come from this enum:
// http://google3/java/com/google/android/gmscore/integ/client/smartdevice/src/com/google/android/gms/smartdevice/d2d/DeviceType.java;l=57;rcl=526500829
constexpr char kDeviceTypeQueryParamValue[] = "7";

}  // namespace

QRCode::QRCode(AdvertisingId advertising_id, SharedSecret shared_secret)
    : advertising_id_(advertising_id), shared_secret_(shared_secret) {}

QRCode::QRCode(const QRCode& other) = default;

QRCode& QRCode::operator=(const QRCode& other) = default;

QRCode::~QRCode() = default;

std::string QRCode::GetQRCodeURLString() {
  std::string shared_secret_str(shared_secret_.begin(), shared_secret_.end());
  std::string shared_secret_base64 = base::Base64Encode(shared_secret_str);
  url::RawCanonOutputT<char> shared_secret_base64_uriencoded;
  url::EncodeURIComponent(shared_secret_base64,
                          &shared_secret_base64_uriencoded);
  return base::StrCat({"https://signin.google/qs/", advertising_id_.ToString(),
                       "?key=", shared_secret_base64_uriencoded.view(),
                       "&t=", kDeviceTypeQueryParamValue});
}

QRCode::PixelData QRCode::GetPixelData() {
  std::string url_string = GetQRCodeURLString();
  std::vector<uint8_t> blob =
      std::vector<uint8_t>(url_string.begin(), url_string.end());
  base::expected<qr_code_generator::GeneratedCode, qr_code_generator::Error>
      generated_code = qr_code_generator::GenerateCode(blob);
  CHECK(generated_code.has_value()) << "generated_code has no value";
  auto res =
      PixelData{generated_code->data.begin(), generated_code->data.end()};
  CHECK_EQ(res.size(), static_cast<size_t>(generated_code->qr_size *
                                           generated_code->qr_size))
      << "unexpected size for QR code data";
  return res;
}

}  // namespace ash::quick_start
