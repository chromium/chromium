// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_connectors {

// Controls whether the local content analysis feature can be used for any
// service provider and/or policy configuration.
BASE_DECLARE_FEATURE(kLocalContentAnalysisEnabled);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
