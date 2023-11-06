// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_SCOPED_PDH_QUERY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_SCOPED_PDH_QUERY_H_

#include <pdh.h>

#include "base/scoped_generic.h"

namespace performance_manager::metrics {

namespace internal {

// Scoped PdhQuery class to maintain lifetime of PDH_HQUERY.
struct ScopedPdhQueryTraits {
  static PDH_HQUERY InvalidValue() { return NULL; }
  static void Free(PDH_HQUERY query) { PdhCloseQuery(query); }
};

}  // namespace internal

// ScopedPdhQuery is a wrapper around a PDH_HQUERY.
//
// Example use:
//
//   ScopedPdhQuery pdh_query = ScopedPdhQuery::Create();
//
// Also:
//
//   PDH_HQUERY pdh_query;
//   PDH_STATUS status = PdhOpenQuery(..., &pdh_query);
//   ScopedPdhQuery pdh_query(pdh_query);
class ScopedPdhQuery
    : public base::ScopedGeneric<PDH_HQUERY, internal::ScopedPdhQueryTraits> {
 public:
  // Initializes with a NULL PDH_HQUERY.
  ScopedPdhQuery();

  // Constructs a ScopedPdhQuery from a PDH_HQUERY, and takes ownership of
  // `pdh_query`.
  explicit ScopedPdhQuery(PDH_HQUERY pdh_query);

  static ScopedPdhQuery Create();
};

}  // namespace performance_manager::metrics

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_SCOPED_PDH_QUERY_H_
