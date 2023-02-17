// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/event_logging_features.h"

#include "base/feature_list.h"

namespace metrics::structured {

BASE_FEATURE(kAppDiscoveryLogging,
             "AppDiscoveryLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

}
