// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_METRICS_H_
#define CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_METRICS_H_

namespace ash {
namespace power {
namespace ml {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

enum class SmartDimModelResult {
  kSuccess = 0,
  kPreprocessorInitializationFailed = 1,
  kPreprocessorOtherError = 2,
  kOtherError = 3,
  kMismatchedFeatureSizeError = 4,
  kMlServiceInitializationFailedError = 5,
  kMaxValue = kMlServiceInitializationFailedError
};

enum class SmartDimParameterResult {
  kSuccess = 0,
  kUndefinedError = 1,
  kParsingError = 2,
  kUseDefaultValue = 3,
  kMaxValue = kUseDefaultValue
};

enum class LoadComponentEvent {
  kSuccess = 0,
  kReadComponentFilesError = 1,
  kLoadPreprocessorError = 2,
  kLoadMetadataError = 3,
  kLoadModelError = 4,
  kCreateGraphExecutorError = 5,
  kMaxValue = kCreateGraphExecutorError
};

enum class WorkerType {
  kBuiltinWorker = 0,
  kDownloadWorker = 1,
  kMaxValue = kDownloadWorker
};

enum class ComponentVersionType {
  kDefault = 0,
  kExperimental = 1,
  kEmpty = 2,
  kMaxValue = kEmpty
};

void LogPowerMLSmartDimModelResult(SmartDimModelResult result);

void LogPowerMLSmartDimParameterResult(SmartDimParameterResult result);

void LogComponentVersionType(ComponentVersionType type);

void LogWorkerType(WorkerType type);

void LogLoadComponentEvent(LoadComponentEvent event);

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_METRICS_H_
