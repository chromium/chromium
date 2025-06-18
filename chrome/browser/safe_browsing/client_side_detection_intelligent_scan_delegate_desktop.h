// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_

#include "components/safe_browsing/content/browser/client_side_detection_host.h"

class PrefService;

namespace safe_browsing {

// Desktop implementation of IntelligentScanDelegate. This class is responsible
// for managing the on-device model for intelligent scanning, including loading,
// observing updates, and executing the model.
// TODO(crbug.com/424104358): Move remaining functions into this class.
class ClientSideDetectionIntelligentScanDelegateDesktop
    : public ClientSideDetectionHost::IntelligentScanDelegate {
 public:
  explicit ClientSideDetectionIntelligentScanDelegateDesktop(PrefService& pref);
  ~ClientSideDetectionIntelligentScanDelegateDesktop() override;

  ClientSideDetectionIntelligentScanDelegateDesktop(
      const ClientSideDetectionIntelligentScanDelegateDesktop&) = delete;
  ClientSideDetectionIntelligentScanDelegateDesktop& operator=(
      const ClientSideDetectionIntelligentScanDelegateDesktop&) = delete;

  // IntelligentScanDelegate implementation.
  bool ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) override;

 private:
  const raw_ref<PrefService> pref_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_DESKTOP_H_
