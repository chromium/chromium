// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace enterprise_connectors {

// Controls uploading scanned data even after a metadata verdict
// is received for content scans.
BASE_DECLARE_FEATURE(kEnableAsyncUploadAfterVerdict);

// Controls whether resumable upload is enabled on consumer scans.
BASE_DECLARE_FEATURE(kEnableResumableUploadOnConsumerScan);

// Controls the number of content analysis requests concurrently uploaded.
BASE_DECLARE_FEATURE_PARAM(size_t, kParallelContentAnalysisRequestCount);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
