// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

bool ClientSideDetectionIntelligentScanDelegateAndroid::
    ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) {
  return false;
}

}  // namespace safe_browsing
