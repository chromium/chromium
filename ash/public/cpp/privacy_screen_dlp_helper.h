// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PRIVACY_SCREEN_DLP_HELPER_H_
#define ASH_PUBLIC_CPP_PRIVACY_SCREEN_DLP_HELPER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Interface for DLP (Data Leak Prevention) ash client in Chrome to interact
// with the PrivacyScreen feature.
class ASH_PUBLIC_EXPORT PrivacyScreenDlpHelper {
 public:
  static PrivacyScreenDlpHelper* Get();

  // Check if privacy screen is supported by the device.
  virtual bool IsSupported() const = 0;

  // Set PrivacyScreen enforcement because of Data Leak Protection.
  virtual void SetEnforced(bool enforced) = 0;

 protected:
  PrivacyScreenDlpHelper();
  virtual ~PrivacyScreenDlpHelper();
  PrivacyScreenDlpHelper(const PrivacyScreenDlpHelper&) = delete;
  PrivacyScreenDlpHelper& operator=(const PrivacyScreenDlpHelper&) = delete;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PRIVACY_SCREEN_DLP_HELPER_H_
