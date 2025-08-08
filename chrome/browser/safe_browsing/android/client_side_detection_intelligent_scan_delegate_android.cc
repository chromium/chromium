// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "base/notimplemented.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

ClientSideDetectionIntelligentScanDelegateAndroid::
    ClientSideDetectionIntelligentScanDelegateAndroid(PrefService& pref)
    : pref_(pref) {}

bool ClientSideDetectionIntelligentScanDelegateAndroid::
    ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) {
  if (!IsEnhancedProtectionEnabled(*pref_)) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionSendIntelligentScanInfoAndroid)) {
    return false;
  }
  return verdict->client_side_detection_type() ==
             ClientSideDetectionType::FORCE_REQUEST &&
         verdict->has_llama_forced_trigger_info() &&
         verdict->llama_forced_trigger_info().intelligent_scan();
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::
    IsOnDeviceModelAvailable(bool log_failed_eligibility_reason) {
  return false;
}

void ClientSideDetectionIntelligentScanDelegateAndroid::InquireOnDeviceModel(
    std::string rendered_texts,
    InquireOnDeviceModelDoneCallback callback) {
  NOTIMPLEMENTED();
  return;
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::ResetOnDeviceSession() {
  return false;
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::ShouldShowScamWarning(
    std::optional<IntelligentScanVerdict> verdict) {
  return false;
}

}  // namespace safe_browsing
