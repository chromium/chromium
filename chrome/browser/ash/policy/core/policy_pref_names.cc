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

}  // namespace prefs
}  // namespace policy
