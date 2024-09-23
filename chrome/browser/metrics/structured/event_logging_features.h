// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_EVENT_LOGGING_FEATURES_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_EVENT_LOGGING_FEATURES_H_

#include "base/feature_list.h"

namespace metrics::structured {

// The number of events that need to be recorded before an upload can occur.
int GetOobeEventUploadCount();

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_EVENT_LOGGING_FEATURES_H_
