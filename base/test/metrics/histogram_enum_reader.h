// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_METRICS_HISTOGRAM_ENUM_READER_H_
#define BASE_TEST_METRICS_HISTOGRAM_ENUM_READER_H_

#include <map>
#include <string>

#include "base/metrics/histogram_base.h"
#include "base/optional.h"

namespace base {

using HistogramEnumEntryMap = std::map<HistogramBase::Sample, std::string>;

// Find and read the enum with the given |enum_name| (with integer values) from
// tools/metrics/histograms/enums.xml.
//
// Returns map { value => label } so that:
//   <int value="9" label="enable-pinch-virtual-viewport"/>
// becomes:
//   { 9 => "enable-pinch-virtual-viewport" }
// Returns empty base::nullopt on failure.
base::Optional<HistogramEnumEntryMap> ReadEnumFromEnumsXml(
    const std::string& enum_name);

}  // namespace base

#endif  // BASE_TEST_METRICS_HISTOGRAM_ENUM_READER_H_
