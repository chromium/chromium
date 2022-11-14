// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_HID_DETECTION_REVAMP_FIELD_TRIAL_H_
#define CHROME_BROWSER_ASH_LOGIN_HID_DETECTION_REVAMP_FIELD_TRIAL_H_

#include "base/metrics/field_trial.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class FeatureList;
}  // namespace base

// TODO(b/258289728): Delete this file once feature is fully launched.
namespace ash::hid_detection_revamp_field_trial {

// String local state preference with the name of the assigned trial group.
// Empty if no group has been assigned yet.
inline constexpr char kTrialGroupPrefName[] =
    "hid_detection_revamp.trial_group";

// The field trial name.
inline constexpr char kTrialName[] = "HidDetectionRevamp";

// Group names for the trial.
inline constexpr char kEnabledGroup[] = "Enabled";
inline constexpr char kDisabledGroup[] = "Disabled";
inline constexpr char kDefaultGroup[] = "Default";

// Registers the local state pref to hold the trial group.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Enables client-side rollout for the kOobeHidDetectionRevamp feature.
//
// kOobeHidDetectionRevamp is an OOBE feature and cannot depend on the
// variation seed being available. See go/launch/4211092 for details on the
// feature.
//
// The rollout plan for this feature is 50% for dev/beta and 1% for stable.
void Create(const base::FieldTrial::EntropyProvider& entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state);

}  // namespace ash::hid_detection_revamp_field_trial

#endif  // CHROME_BROWSER_ASH_LOGIN_HID_DETECTION_REVAMP_FIELD_TRIAL_H_
