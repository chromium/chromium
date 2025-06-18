// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_ANDROID_H_

#include "components/safe_browsing/content/browser/client_side_detection_host.h"

namespace safe_browsing {

// Android implementation of IntelligentScanDelegate. This class is responsible
// for managing sessions and executing the model.
// TODO(crbug.com/424075615): Implement this class.
class ClientSideDetectionIntelligentScanDelegateAndroid
    : public ClientSideDetectionHost::IntelligentScanDelegate {
 public:
  ClientSideDetectionIntelligentScanDelegateAndroid() = default;
  ~ClientSideDetectionIntelligentScanDelegateAndroid() override = default;

  ClientSideDetectionIntelligentScanDelegateAndroid(
      const ClientSideDetectionIntelligentScanDelegateAndroid&) = delete;
  ClientSideDetectionIntelligentScanDelegateAndroid& operator=(
      const ClientSideDetectionIntelligentScanDelegateAndroid&) = delete;

  // IntelligentScanDelegate implementation.
  bool ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_ANDROID_H_
