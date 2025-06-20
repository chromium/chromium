// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"

#include "base/feature_list.h"

namespace enterprise_connectors {

BASE_FEATURE(kEnableAsyncUploadAfterVerdict,
             "EnableAsyncUploadAfterVerdict",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableResumableUploadOnConsumerScan,
             "EnableResumableUploadOnConsumerScan",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The default value 5 is set for historical reasons, there is no reasoning
// behind it. This finch flag will be used to help determine a more suitable
// value.
BASE_FEATURE_PARAM(size_t,
                   kParallelContentAnalysisRequestCount,
                   &kEnableAsyncUploadAfterVerdict,
                   "max_parallel_requests",
                   /*default_value=*/5);

// Controls the new upload/download limit for content analysis.
BASE_FEATURE(kEnableNewUploadSizeLimit,
             "EnableNewUploadSizeLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kMaxContentAnalysisFileSizeMB,
                   &kEnableNewUploadSizeLimit,
                   "max_file_size_mb",
                   /*default_value=*/50);

// Controls whether encrypted file upload is enabled.
BASE_FEATURE(kEnableEncryptedFileUpload,
             "EnableEncryptedFileUpload",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace enterprise_connectors
