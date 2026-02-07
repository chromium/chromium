// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/report_unsafe_site_dialog.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace feedback {

// static
bool ReportUnsafeSiteDialog::IsEnabled(const Profile& profile) {
  const PrefService* prefs = profile.GetPrefs();
  return base::FeatureList::IsEnabled(features::kReportUnsafeSite) &&
         prefs->GetBoolean(prefs::kUserFeedbackAllowed) &&
         safe_browsing::IsSafeBrowsingEnabled(*prefs);
}

}  // namespace feedback
