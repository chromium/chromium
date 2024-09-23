// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_connectors {

// Controls whether the custom message per rule feature is enabled or not. Does
// not affect interstitials custom message per rule feature.
BASE_DECLARE_FEATURE(kDialogCustomRuleMessageEnabled);

// Controls whether the resumable upload protocol is enabled or not.
BASE_DECLARE_FEATURE(kResumableUploadEnabled);

// Returns true if custom rule message is enabled.
bool IsDialogCustomRuleMessageEnabled();

// Returns true if resumable upload is enabled.
bool IsResumableUploadEnabled();

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
