// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define PRODUCT_NAME L"Google\\Chrome"
#elif BUILDFLAG(CHROMIUM_BRANDING)
#define PRODUCT_NAME L"Chromium"
#else
#error Unknown branding
#endif

// The registry path where the TriggeredReset values get set. Note that this
// uses the same path for both SxS (Canary) and non-SxS Chrome. This is
// intended to allow third parties to use the API without needing to be
// aware of and maintain changes to Chrome's channel logic.
const wchar_t kTriggeredResetRegistryPath[] =
    L"Software\\" PRODUCT_NAME L"\\TriggeredReset";

const wchar_t kTriggeredResetToolName[] = L"ToolName";
const wchar_t kTriggeredResetTimestamp[] = L"Timestamp";

namespace {

const char kTriggeredResetFieldTrialName[] = "TriggeredResetFieldTrial";
const char kTriggeredResetOffGroup[] = "Off";

bool IsDisabledByFieldTrial() {
  return base::FieldTrialList::FindFullName(kTriggeredResetFieldTrialName) ==
         kTriggeredResetOffGroup;
}

}  // namespace

void TriggeredProfileResetter::Activate() {
  activate_called_ = true;

  // System profiles don't contain user settings and bail out if we're not in
  // the field trial.
  if (!profile_ || profile_->IsSystemProfile() || IsDisabledByFieldTrial()) {
    UMA_HISTOGRAM_BOOLEAN("Profile.TriggeredReset", false);
    return;
  }

  int64_t timestamp = 0;
  base::win::RegKey reset_reg_key(HKEY_CURRENT_USER,
                                  kTriggeredResetRegistryPath, KEY_QUERY_VALUE);

  if (!reset_reg_key.Valid() ||
      reset_reg_key.ReadInt64(kTriggeredResetTimestamp, &timestamp) !=
          ERROR_SUCCESS) {
    UMA_HISTOGRAM_BOOLEAN("Profile.TriggeredReset", false);
    return;
  }

  // A reset trigger time was found. Compare it to the trigger time stored
  // in this profile. If different, reset the profile and persist the new
  // time.
  PrefService* pref_service = profile_->GetPrefs();
  const int64_t preference_timestamp =
      pref_service->GetInt64(prefs::kLastProfileResetTimestamp);

  if (profile_->IsNewProfile()) {
    // New profiles should never be reset. Instead, persist the time stamp
    // directly.
    pref_service->SetInt64(prefs::kLastProfileResetTimestamp, timestamp);
  } else if (timestamp != preference_timestamp) {
    DVLOG(1) << "Profile reset detected.";

    has_reset_trigger_ = true;

    if (reset_reg_key.ReadValue(kTriggeredResetToolName, &tool_name_) !=
        ERROR_SUCCESS) {
      DVLOG(1) << "Failed to read triggered profile reset tool name.";
    } else if (tool_name_.length() > kMaxToolNameLength) {
      tool_name_.resize(kMaxToolNameLength);
    }

    pref_service->SetInt64(prefs::kLastProfileResetTimestamp, timestamp);
  }

  UMA_HISTOGRAM_BOOLEAN("Profile.TriggeredReset", has_reset_trigger_);
}
