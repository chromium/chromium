// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"

namespace enterprise_connectors {

BASE_FEATURE(kResumableUploadEnabled,
             "ResumableUploadEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsResumableUploadEnabled() {
  return base::FeatureList::IsEnabled(kResumableUploadEnabled);
}

}  // namespace enterprise_connectors
