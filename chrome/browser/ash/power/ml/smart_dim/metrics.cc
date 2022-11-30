// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/smart_dim/metrics.h"

#include "base/metrics/histogram_macros.h"

namespace ash {
namespace power {
namespace ml {

void LogPowerMLSmartDimModelResult(SmartDimModelResult result) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.SmartDimModel.Result", result);
}

void LogPowerMLSmartDimParameterResult(SmartDimParameterResult result) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.SmartDimParameter.Result", result);
}

void LogLoadComponentEvent(LoadComponentEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.SmartDimComponent.LoadComponentEvent",
                            event);
}

void LogWorkerType(WorkerType type) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.SmartDimComponent.WorkerType", type);
}

void LogComponentVersionType(ComponentVersionType type) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.SmartDimComponent.VersionType", type);
}

}  // namespace ml
}  // namespace power
}  // namespace ash
