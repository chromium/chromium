// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"

// PII (Personally Identifiable Information) types that can exist in the debug
// data and logs DataCollector collects.
enum class PIIType {
  // Window titles that appear in UI hierarchy.
  kUIHierarchyWindowTitles,
};

using PIIMap = std::multimap<PIIType, std::string>;

// The DataCollector provides an interface for data sources that the
// SupportToolHandler uses to collect debug data from multiple sources in Chrome
// or Chrome OS with Support Tool.
class DataCollector {
 public:
  virtual ~DataCollector() = default;

  // Returns a url-encodable name of this DataCollector.
  virtual std::string GetName() const = 0;

  // Returns a user-readable description of what this DataCollector does.
  virtual std::string GetDescription() const = 0;

  // Returns the detected PII in the collected data.
  virtual const PIIMap& GetDetectedPII() = 0;

  // Collects all data that can be collected and detects the PII in the
  // collected data.
  virtual void CollectDataAndDetectPII(
      base::OnceClosure on_data_collected_callback) = 0;
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_
