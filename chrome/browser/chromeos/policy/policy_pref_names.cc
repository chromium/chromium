// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_pref_names.h"

namespace prefs {

// Maintain a list of last upload times of system logs in double type; this is
// for the purpose of throttling log uploads.
const char kStoreLogStatesAcrossReboots[] =
    "policy_store_log_states_across_reboots";

}  // namespace prefs
