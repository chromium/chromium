// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_desktop.h"

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

ClientSideDetectionIntelligentScanDelegateDesktop::
    ClientSideDetectionIntelligentScanDelegateDesktop(PrefService& pref)
    : pref_(pref) {}

ClientSideDetectionIntelligentScanDelegateDesktop::
    ~ClientSideDetectionIntelligentScanDelegateDesktop() = default;

bool ClientSideDetectionIntelligentScanDelegateDesktop::
    ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) {
  if (!IsEnhancedProtectionEnabled(*pref_)) {
    return false;
  }

  bool is_keyboard_lock_requested =
      base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection) &&
      verdict->client_side_detection_type() ==
          ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED;

  bool is_intelligent_scan_requested =
      base::FeatureList::IsEnabled(
          kClientSideDetectionLlamaForcedTriggerInfoForScamDetection) &&
      verdict->has_llama_forced_trigger_info() &&
      verdict->llama_forced_trigger_info().intelligent_scan();

  return is_keyboard_lock_requested || is_intelligent_scan_requested;
}

}  // namespace safe_browsing
