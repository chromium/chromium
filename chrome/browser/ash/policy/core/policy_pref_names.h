// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_POLICY_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_POLICY_PREF_NAMES_H_

namespace policy::prefs {

// Maintain a list of last upload times of system logs in double type; this is
// for the purpose of throttling log uploads.
inline constexpr char kStoreLogStatesAcrossReboots[] =
    "policy_store_log_states_across_reboots";

// A preference to keep track of upload times of event based logs.
inline constexpr char kEventBasedLogLastUploadTimes[] =
    "ash.policy.event_based_log_last_upload_times";

}  // namespace policy::prefs

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_POLICY_PREF_NAMES_H_
