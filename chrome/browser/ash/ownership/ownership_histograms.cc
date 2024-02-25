// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ownership/ownership_histograms.h"

#include "base/metrics/histogram_functions.h"

namespace ash {
namespace {

OwnerKeyUmaEvent ConvertToUmaEvent(OwnerKeyEvent event, bool success) {
  if (success) {
    switch (event) {
      case OwnerKeyEvent::kDeviceSettingsServiceIsNull:
        return OwnerKeyUmaEvent::kDeviceSettingsServiceIsNull;
      case OwnerKeyEvent::kManagedDevice:
        return OwnerKeyUmaEvent::kManagedDeviceSuccess;
      case OwnerKeyEvent::kOwnerHasKeys:
        return OwnerKeyUmaEvent::kOwnerHasKeysSuccess;
      case OwnerKeyEvent::kEstablishingConsumerOwnership:
        return OwnerKeyUmaEvent::kEstablishingConsumerOwnershipSuccess;
      case OwnerKeyEvent::kRegeneratingOwnerKeyBasedOnPolicy:
        return OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnPolicySuccess;
      case OwnerKeyEvent::kUserNotAnOwnerBasedOnPolicy:
        return OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnPolicySuccess;
      case OwnerKeyEvent::kRegeneratingOwnerKeyBasedOnLocalState:
        return OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnLocalStateSuccess;
      case OwnerKeyEvent::kUserNotAnOwnerBasedOnLocalState:
        return OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnLocalStateSuccess;
      case OwnerKeyEvent::kUnsureUserNotAnOwner:
        return OwnerKeyUmaEvent::kUnsureUserNotAnOwnerSuccess;
      case OwnerKeyEvent::kOwnerKeyGenerated:
        return OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess;
      case OwnerKeyEvent::kFailedToGenerateOwnerKey:
        return OwnerKeyUmaEvent::kFailedToGenerateOwnerKeySuccess;
      case OwnerKeyEvent::kStartSigningPolicy:
        return OwnerKeyUmaEvent::kStartSigningPolicySuccess;
      case OwnerKeyEvent::kSignedPolicy:
        return OwnerKeyUmaEvent::kSignedPolicySuccess;
      case OwnerKeyEvent::kStoredPolicy:
        return OwnerKeyUmaEvent::kStoredPolicySuccess;
      case OwnerKeyEvent::kUserNotAnOwnerBasedOnUserType:
        return OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnUserTypeSuccess;
      case OwnerKeyEvent::kUserNotAnOwnerBasedOnEmptyUsername:
        return OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnEmptyUsernameSuccess;
      case OwnerKeyEvent::kUnsureTakeOwnership:
        return OwnerKeyUmaEvent::kUnsureTakeOwnership;
      case OwnerKeyEvent::kPrivateSlotKeyGeneration:
        return OwnerKeyUmaEvent::kPrivateSlotKeyGenerationSuccess;
      case OwnerKeyEvent::kPublicSlotKeyGeneration:
        return OwnerKeyUmaEvent::kPublicSlotKeyGenerationSuccess;
      case OwnerKeyEvent::kMigrationToPrivateSlotStarted:
        return OwnerKeyUmaEvent::kMigrationToPrivateSlotStarted;
      case OwnerKeyEvent::kMigrationToPublicSlotStarted:
        return OwnerKeyUmaEvent::kMigrationToPublicSlotStarted;
      case OwnerKeyEvent::kOwnerKeySet:
        return OwnerKeyUmaEvent::kOwnerKeySetSuccess;
      case OwnerKeyEvent::kOldOwnerKeyCleanUpStarted:
        return OwnerKeyUmaEvent::kOldOwnerKeyCleanUpStarted;
      case OwnerKeyEvent::kOwnerKeyInPublicSlot:
        return OwnerKeyUmaEvent::kOwnerKeyInPublicSlotTrue;
    }
  } else {
    switch (event) {
      case OwnerKeyEvent::kDeviceSettingsServiceIsNull:
        return OwnerKeyUmaEvent::kDeviceSettingsServiceIsNull;
      case OwnerKeyEvent::kManagedDevice:
        return OwnerKeyUmaEvent::kManagedDeviceFail;
      case OwnerKeyEvent::kOwnerHasKeys:
        return OwnerKeyUmaEvent::kOwnerHasKeysFail;
      case OwnerKeyEvent::kEstablishingConsumerOwnership:
        return OwnerKeyUmaEvent::kEstablishingConsumerOwnershipFail;
      case OwnerKeyEvent::kRegeneratingOwnerKeyBasedOnPolicy:
        return OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnPolicyFail;
      case OwnerKeyEvent::kUserNotAnOwnerBasedOnPolicy:
        return OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnPolicyFail;
      case OwnerKeyEvent::kRegeneratingOwnerKeyBasedOnLocalState:
        return OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnLocalStateFail;
      case OwnerKeyEvent::kUserNotAnOwnerBasedOnLocalState:
        return OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnLocalStateFail;
      case OwnerKeyEvent::kUnsureUserNotAnOwner:
        return OwnerKeyUmaEvent::kUnsureUserNotAnOwnerFail;
      case OwnerKeyEvent::kOwnerKeyGenerated:
        return OwnerKeyUmaEvent::kOwnerKeyGeneratedFail;
      case OwnerKeyEvent::kFailedToGenerateOwnerKey:
        return OwnerKeyUmaEvent::kFailedToGenerateOwnerKeyFail;
      case OwnerKeyEvent::kStartSigningPolicy:
        return OwnerKeyUmaEvent::kStartSigningPolicyFail;
      case OwnerKeyEvent::kSignedPolicy:
        return OwnerKeyUmaEvent::kSignedPolicyFail;
      case OwnerKeyEvent::kStoredPolicy:
        return OwnerKeyUmaEvent::kStoredPolicyFail;
      case OwnerKeyEvent::kUserNotAnOwnerBasedOnUserType:
        return OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnUserTypeFail;
      case OwnerKeyEvent::kUserNotAnOwnerBasedOnEmptyUsername:
        return OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnEmptyUsernameFail;
      case OwnerKeyEvent::kUnsureTakeOwnership:
        return OwnerKeyUmaEvent::kUnsureTakeOwnership;
      case OwnerKeyEvent::kPrivateSlotKeyGeneration:
        return OwnerKeyUmaEvent::kPrivateSlotKeyGenerationFail;
      case OwnerKeyEvent::kPublicSlotKeyGeneration:
        return OwnerKeyUmaEvent::kPublicSlotKeyGenerationFail;
      case OwnerKeyEvent::kMigrationToPrivateSlotStarted:
        return OwnerKeyUmaEvent::kMigrationToPrivateSlotStarted;
      case OwnerKeyEvent::kMigrationToPublicSlotStarted:
        return OwnerKeyUmaEvent::kMigrationToPublicSlotStarted;
      case OwnerKeyEvent::kOwnerKeySet:
        return OwnerKeyUmaEvent::kOwnerKeySetFail;
      case OwnerKeyEvent::kOldOwnerKeyCleanUpStarted:
        return OwnerKeyUmaEvent::kOldOwnerKeyCleanUpStarted;
      case OwnerKeyEvent::kOwnerKeyInPublicSlot:
        return OwnerKeyUmaEvent::kOwnerKeyInPublicSlotFalse;
    }
  }
}

}  // namespace

void RecordOwnerKeyEvent(OwnerKeyEvent event, bool success) {
  base::UmaHistogramEnumeration(kOwnerKeyHistogramName,
                                ConvertToUmaEvent(event, success));
}

}  // namespace ash
