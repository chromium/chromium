// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"

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
  // collected data. Please bind the `on_data_collected_callback` with WeakPtr
  // when creating it to make sure it won't be run when the caller instance is
  // deleted.
  virtual void CollectDataAndDetectPII(
      base::OnceClosure on_data_collected_callback) = 0;

  // Masks all PII found in the collected data except `pii_types_to_keep`.
  // Exports the collected data into file(s) in `target_directory`. Calls
  // `on_exported_callback` when done. `on_exported_callback` may still be
  // called even if the class got deleted. You may use WeakPtr when creating
  // the callback for this function.
  // TODO(b/200510719): This will be changed to not call the callback after the
  // instance has been deleted in https://crrev.com/c/3268906.
  virtual void ExportCollectedDataWithPII(
      std::set<PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      base::OnceClosure on_exported_callback) = 0;
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_
