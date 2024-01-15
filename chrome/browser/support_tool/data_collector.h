// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

// The error code that a Support Tool component can return.
enum class SupportToolErrorCode {
  // Errors that occurred in individual DataCollector level. An error in a
  // DataCollector instance won't disturb the execution of the caller of
  // DataCollector functions.
  kDataCollectorError,
  // Errors that occur during the data export process of the caller of
  // DataCollector instances.
  kDataExportError,
};

// SupportToolError is an error that a Support Tool component can return.
struct SupportToolError {
  // General meaning error code.
  SupportToolErrorCode error_code;
  // Detailed error message.
  std::string error_message;

  SupportToolError(SupportToolErrorCode error_code, std::string error_message)
      : error_code(error_code), error_message(error_message) {}

  // We need to overload < operator since it will be used when SupportToolErrors
  // are added to std::set to be returned in SupportToolHandler.
  bool operator<(const SupportToolError& other) const {
    return std::tie(error_code, error_message) <
           std::tie(other.error_code, other.error_message);
  }
};

using PIIMap = std::map<redaction::PIIType, std::set<std::string>>;

// Returns a SupportToolError if an error occurs to the callback.
using DataCollectorDoneCallback =
    base::OnceCallback<void(std::optional<SupportToolError> error)>;

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
  // collected data. DataCollector may use redaction::RedactionTool of
  // `redaction_tool_container` on `task_runner_for_redaction_tool` for PII
  // detection or implement their own PII detection functions.
  // `on_data_collected_callback` won't be run if the DataCollector instance is
  // deleted.
  virtual void CollectDataAndDetectPII(
      DataCollectorDoneCallback on_data_collected_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer>
          redaction_tool_container) = 0;

  // Masks all PII found in the collected data except `pii_types_to_keep`.
  // Exports the collected data into file(s) in `target_directory`. Calls
  // `on_exported_callback` when done. `on_exported_callback` won't be called
  // if the DataCollector instance is deleted.
  virtual void ExportCollectedDataWithPII(
      std::set<redaction::PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      DataCollectorDoneCallback on_exported_callback) = 0;
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_DATA_COLLECTOR_H_
