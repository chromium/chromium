// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/cpu_probe/scoped_pdh_query.h"

#include "base/logging.h"

namespace performance_manager::metrics {

ScopedPdhQuery::ScopedPdhQuery() = default;

ScopedPdhQuery::ScopedPdhQuery(PDH_HQUERY pdh_query)
    : ScopedGeneric(pdh_query) {}

// static
ScopedPdhQuery ScopedPdhQuery::Create() {
  PDH_HQUERY pdh_query;
  PDH_STATUS pdh_status = PdhOpenQuery(NULL, NULL, &pdh_query);
  if (pdh_status == ERROR_SUCCESS) {
    return ScopedPdhQuery(std::move(pdh_query));
  } else {
    LOG(ERROR) << "PdhOpenQuery failed: "
               << logging::SystemErrorCodeToString(pdh_status);
    return ScopedPdhQuery();
  }
}

}  // namespace performance_manager::metrics
