// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OWNERSHIP_OWNERSHIP_HISTOGRAMS_H_
#define CHROME_BROWSER_ASH_OWNERSHIP_OWNERSHIP_HISTOGRAMS_H_

namespace ash {

// Events related to owner key loading, generation and usage.
enum class OwnerKeyEvent {
  // DeviceSettingsService was null, owner key was not loaded.
  kDeviceSettingsServiceIsNull,
  // Managed device finished key loading.
  kManagedDevice,
  // Consumer device finished key loading.
  kOwnerHasKeys,
  // ChromeOS decided to establish consumer ownership.
  kEstablishingConsumerOwnership,
  // ChromeOS decided to re-generate the lost owner key based on the data from
  // device policies.
  kRegeneratingOwnerKeyBasedOnPolicy,
  // A user was categorized as not an owner based on the data from device
  // policies.
  kUserNotAnOwnerBasedOnPolicy,
  // ChromeOS decided to re-generate the lost owner key based on the data from
  // local state.
  kRegeneratingOwnerKeyBasedOnLocalState,
  // A user was categorized as not an owner based on the data from local state.
  kUserNotAnOwnerBasedOnLocalState,
  // ChromeOS assumed that a user is not an owner based on the lack of data.
  kUnsureUserNotAnOwner,
  // New owner key was generated.
  kOwnerKeyGenerated,
  // Failed to generate new owner key.
  kFailedToGenerateOwnerKey,
  // Started signing policies.
  kStartSigningPolicy,
  // Finished signing policies.
  kSignedPolicy,
  // Finished storing policies.
  kStoredPolicy,
};

// Combines `event` and `success` to produce a more specific UMA event and
// records it. `success`==true generally means that the event happened as
// expected and `success`==false means that something related to the event went
// wrong or unexpectedly (see comments for the UMA events for more details).
void RecordOwnerKeyEvent(OwnerKeyEvent event, bool success);

// PUBLIC ONLY FOR TESTING:

// The path for the OwnerKeyUmaEvent histogram. Accessible for testing. Prefer
// using through the  `RecordOwnerKeyEvent` method.
constexpr char kOwnerKeyHistogramName[] = "ChromeOS.Ownership.OwnerKeyUmaEvent";

// Events related to owner key loading, generation and usage that are sent to
// UMA. Produced from the events above by combining with a success/failure
// status. These values are persisted to histograms. Entries should not be
// renumbered and numeric values should never be reused.
enum class OwnerKeyUmaEvent {
  // DeviceSettingsService was null, owner key was not loaded.
  kDeviceSettingsServiceIsNull = 0,
  // Managed device successfully loaded the public owner key.
  kManagedDeviceSuccess = 1,
  // Managed device failed to load the public owner key.
  kManagedDeviceFail = 2,
  // Consumer owner user successfully loaded both public and private keys.
  kOwnerHasKeysSuccess = 3,
  // Consumer owner received both public and private keys, but at least one of
  // them wasn't actually loaded.
  kOwnerHasKeysFail = 4,
  // ChromeOS decided to establish consumer ownership when there was no existing
  // public key.
  kEstablishingConsumerOwnershipSuccess = 5,
  // ChromeOS decided to establish consumer ownership when there was an existing
  // public key.
  kEstablishingConsumerOwnershipFail = 6,
  // ChromeOS decided to re-generate the lost owner key based on the data from
  // device policies after the public key was found (the private part is what
  // was lost).
  kRegeneratingOwnerKeyBasedOnPolicySuccess = 7,
  // ChromeOS decided to re-generate the lost owner key based on the data from
  // device policies and the public key was also not found. (Strictly speaking
  // not a failure, but still an unusual situation).
  kRegeneratingOwnerKeyBasedOnPolicyFail = 8,
  // A user was categorized as not an owner based on the data from device
  // policies, the public key was successfully loaded.
  kUserNotAnOwnerBasedOnPolicySuccess = 9,
  // A user was categorized as not an owner based on the data from device
  // policies, the public key failed to load.
  kUserNotAnOwnerBasedOnPolicyFail = 10,
  // ChromeOS decided to re-generate the lost owner key based on the data from
  // local state and the public key was not present.
  kRegeneratingOwnerKeyBasedOnLocalStateSuccess = 11,
  // ChromeOS decided to re-generate the lost owner key based on the data from
  // local state after the public key was found (in such a case device policies
  // should be used, relying on local state is unexpected).
  kRegeneratingOwnerKeyBasedOnLocalStateFail = 12,
  // A user was categorized as not an owner based on the data from local state,
  // the public key was successfully loaded.
  kUserNotAnOwnerBasedOnLocalStateSuccess = 13,
  // A user was categorized as not an owner based on the data from local state,
  // the public key failed to load.
  kUserNotAnOwnerBasedOnLocalStateFail = 14,
  // ChromeOS assumed that a user is not an owner based on the lack of data, the
  // public key was successfully loaded.
  kUnsureUserNotAnOwnerSuccess = 15,
  // ChromeOS assumed that a user is not an owner based on the lack of data, the
  // public key failed to load.
  kUnsureUserNotAnOwnerFail = 16,
  // New owner key was generated on the first attempt.
  kOwnerKeyGeneratedSuccess = 17,
  // New owner key was generated after 1+ failures.
  kOwnerKeyGeneratedFail = 18,
  // Failed to generate new owner key, at least the old public key was returned.
  kFailedToGenerateOwnerKeySuccess = 19,
  // Failed to generate new owner key, the old public key also failed to load
  // (or was not present).
  kFailedToGenerateOwnerKeyFail = 20,
  // Successfully started signing policies.
  kStartSigningPolicySuccess = 21,
  // Failed to start signing policies.
  kStartSigningPolicyFail = 22,
  // Successfully signed policies.
  kSignedPolicySuccess = 23,
  // Failed to sign policies.
  kSignedPolicyFail = 24,
  // Successfully stored policies.
  kStoredPolicySuccess = 25,
  // Failed to store policies.
  kStoredPolicyFail = 26,
  kMaxValue = kStoredPolicyFail,
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OWNERSHIP_OWNERSHIP_HISTOGRAMS_H_
