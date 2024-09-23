// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_QR_CODE_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_QR_CODE_H_

#include <array>
#include <vector>

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/advertising_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"

namespace ash::quick_start {

// Generates and stores QR Code data used to verify the Quick Start connection.
class QRCode {
 public:
  using PixelData = std::vector<uint8_t>;
  using SharedSecret = SessionContext::SharedSecret;

  QRCode(AdvertisingId advertising_id, SharedSecret shared_secret);
  QRCode(const QRCode& other);
  QRCode& operator=(const QRCode& other);
  ~QRCode();

  std::string GetQRCodeURLString();
  PixelData GetPixelData();

 private:
  AdvertisingId advertising_id_;
  SharedSecret shared_secret_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_QR_CODE_H_
