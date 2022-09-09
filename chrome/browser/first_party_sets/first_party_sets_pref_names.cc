// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"

#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace first_party_sets {

// *************** PROFILE PREFS ***************

// A boolean pref indicating whether First-Party Sets is enabled. Exposed to the
// user via Chrome UI, and to enterprise via policy.
const char kFirstPartySetsEnabled[] = "first_party_sets.enabled";

// A dictionary pref that can contain up to two lists of First-Party Sets that
// enterprises can use to override the list of First-Party Sets by either
// replacing or adding to the existing list.
const char kFirstPartySetsOverrides[] = "first_party_sets.overrides";

}  // namespace first_party_sets
