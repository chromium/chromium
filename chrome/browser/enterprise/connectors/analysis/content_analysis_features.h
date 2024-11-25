// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_connectors {

// Controls whether Chrome can stop register fcm token.
BASE_DECLARE_FEATURE(kStopRegisterFcmEnabled);

// Returns true if stop register fcm token is enabled.
bool IsStopRegisterFcmEnabled();

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_FEATURES_H_
