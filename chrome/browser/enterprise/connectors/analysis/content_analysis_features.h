// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_connectors {

// Controls whether the resumable upload protocol is enabled or not.
BASE_DECLARE_FEATURE(kResumableUploadEnabled);

// Returns true if resumable upload is enabled.
bool IsResumableUploadEnabled();

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
