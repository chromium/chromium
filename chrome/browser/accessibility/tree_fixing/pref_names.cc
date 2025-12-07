// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/pref_names.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace tree_fixing {

void InitOffTheRecordPrefs(Profile* off_the_record_profile) {
  DCHECK(off_the_record_profile->IsOffTheRecord());
  off_the_record_profile->GetPrefs()->SetBoolean(
      prefs::kAccessibilityAXTreeFixingEnabled, false);
}

}  // namespace tree_fixing
