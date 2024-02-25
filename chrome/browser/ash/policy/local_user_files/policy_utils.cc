// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/local_user_files/policy_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace policy::local_user_files {

bool LocalUserFilesAllowed() {
  // If the flag is disabled, ignore the policy value and allow local storage.
  if (!base::FeatureList::IsEnabled(features::kSkyVault)) {
    return true;
  }
  return g_browser_process->local_state()->GetBoolean(
      prefs::kLocalUserFilesAllowed);
}

}  // namespace policy::local_user_files
