// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"

namespace enterprise_connectors {

BASE_FEATURE(kStopRegisterFcmEnabled,
             "StopRegisterFcmEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableAsyncUploadAfterVerdict,
             "EnableAsyncUploadAfterVerdict",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableResumableUploadOnConsumerScan,
             "EnableResumableUploadOnConsumerScan",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsStopRegisterFcmEnabled() {
  return base::FeatureList::IsEnabled(kStopRegisterFcmEnabled);
}

}  // namespace enterprise_connectors
