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
#include "components/feedback/pii_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// The error code that a Support Tool component can return.
enum class SupportToolError {
  kUIHierarchyDataCollectorError,
  // Error for testing.
  kTestDataCollectorError,
  kDataExportTempDirCreationFailed,
  kDataExportCreateArchiveFailed,
};

using PIIMap = std::map<feedback::PIIType, std::set<std::string>>;

// Returns a SupportToolError if an error occurs to the callback.
using DataCollectorDoneCallback =
    base::OnceCallback<void(absl::optional<SupportToolError> error_code)>;

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
  // collected data. `on_data_collected_callback` won't be run if the
  // DataCollector instance is deleted.
  virtual void CollectDataAndDetectPII(
      DataCollectorDoneCallback on_data_collected_callback) = 0;

  // Masks all PII found in the collected data except `pii_types_to_keep`.
  // Exports the collected data into file(s) in `target_directory`. Calls
  // `on_exported_callback` when done. `on_exported_callback` won't be called
  // if the DataCollector instance is deleted.
  virtual void ExportCollectedDataWithPII(
      std::set<feedback::PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      DataCollectorDoneCallback on_exported_callback) = 0;
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_
