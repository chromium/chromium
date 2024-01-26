// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"

namespace enterprise_connectors {

BASE_FEATURE(kLocalContentAnalysisEnabled,
             "LocalContentAnalysisEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDialogCustomRuleMessageEnabled,
             "DialogCustomRuleMessageEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDialogCustomRuleMessageEnabled() {
  return base::FeatureList::IsEnabled(kDialogCustomRuleMessageEnabled);
}

}  // namespace enterprise_connectors
