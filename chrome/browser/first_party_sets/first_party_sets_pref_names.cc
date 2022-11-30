// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"

namespace first_party_sets {

// *************** PROFILE PREFS ***************

// A dictionary pref that can contain up to two lists of First-Party Sets that
// enterprises can use to override the list of First-Party Sets by either
// replacing or adding to the existing list.
const char kFirstPartySetsOverrides[] = "first_party_sets.overrides";

}  // namespace first_party_sets
