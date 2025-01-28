// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_GENAI_MODEL_HANDLER_H_
#define CHROME_BROWSER_PERMISSIONS_GENAI_MODEL_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"

class OptimizationGuideKeyedService;

namespace permissions {
class GenAiModelHandler
    : public optimization_guide::OnDeviceModelAvailabilityObserver {
 public:
  explicit GenAiModelHandler(OptimizationGuideKeyedService* optimization_guide);
  ~GenAiModelHandler() override;
  GenAiModelHandler(const GenAiModelHandler&) = delete;
  GenAiModelHandler& operator=(const GenAiModelHandler&) = delete;

 private:
  // optimization_guide::OnDeviceModelAvailabilityObserver
  void OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason reason) override;

  // The underlying session provided by optimization guide component.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_;
};
}  // namespace permissions

#endif  // CHROME_BROWSER_PERMISSIONS_GENAI_MODEL_HANDLER_H_
