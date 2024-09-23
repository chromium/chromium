// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/policy_pref_names.h"

namespace policy {
namespace prefs {

// Maintain a list of last upload times of system logs in double type; this is
// for the purpose of throttling log uploads.
const char kStoreLogStatesAcrossReboots[] =
    "policy_store_log_states_across_reboots";

// A preference to keep track of upload times of event based logs.
// TODO: b/330675569 - Register the pref in `DeviceCloudPolicyManagerAsh` when
// `EventBasedLogManager` is created there.
const char kEventBasedLogLastUploadTimes[] =
    "ash.policy.event_based_log_last_upload_times";

}  // namespace prefs
}  // namespace policy
