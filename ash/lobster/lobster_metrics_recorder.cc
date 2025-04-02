// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_metrics_recorder.h"

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

void RecordLobsterState(LobsterMetricState state) {
  base::UmaHistogramEnumeration("Ash.Lobster.State", state);
}

void RecordLobsterBlockedReason(LobsterSystemCheck failed_reason) {
  switch (failed_reason) {
    case LobsterSystemCheck::kMinValue:
      return;
    case LobsterSystemCheck::kInvalidConsent:
      RecordLobsterState(LobsterMetricState::kBlockedByConsent);
      return;
    case LobsterSystemCheck::kInvalidAccountCapabilities:
      RecordLobsterState(LobsterMetricState::kBlockedByAccountCapabilities);
      return;
    case LobsterSystemCheck::kInvalidAccountType:
      RecordLobsterState(LobsterMetricState::kBlockedByAccountType);
      return;
    case LobsterSystemCheck::kInvalidRegion:
      RecordLobsterState(LobsterMetricState::kBlockedByGeolocation);
      return;
    case LobsterSystemCheck::kInvalidInputField:
      RecordLobsterState(LobsterMetricState::kBlockedByInputField);
      return;
    case LobsterSystemCheck::kSettingsOff:
      RecordLobsterState(LobsterMetricState::kBlockedBySettings);
      return;
    case LobsterSystemCheck::kNoInternetConnection:
      RecordLobsterState(LobsterMetricState::kBlockedByInternetConnection);
      return;
    case LobsterSystemCheck::kInvalidInputMethod:
      RecordLobsterState(LobsterMetricState::kBlockedByInputMethod);
      return;
    case LobsterSystemCheck::kInvalidFeatureFlags:
      RecordLobsterState(LobsterMetricState::kBlockedByFeatureFlags);
      return;
    case LobsterSystemCheck::kUnsupportedHardware:
      RecordLobsterState(LobsterMetricState::kBlockedByHardware);
      return;
    case LobsterSystemCheck::kUnsupportedInKioskMode:
      RecordLobsterState(LobsterMetricState::kBlockedByKioskMode);
      return;
    case LobsterSystemCheck::kUnsupportedFormFactor:
      RecordLobsterState(LobsterMetricState::kBlockedByFormFactor);
      return;
    case LobsterSystemCheck::kUnsupportedPolicy:
      RecordLobsterState(LobsterMetricState::kBlockedByPolicy);
      return;
    case LobsterSystemCheck::kForcedDisabledOnManagedUsers:
      // TODO: b:407471938 - add the relevant blocked metrics.
      return;
  }
}

}  // namespace ash
