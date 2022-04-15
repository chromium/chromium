// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_DEFAULT_MODEL_LOW_USER_ENGAGEMENT_MODEL_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_DEFAULT_MODEL_LOW_USER_ENGAGEMENT_MODEL_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Segmentation low engagement model provider. Provides a default model and
// metadata for the low user engagement optimization target.
class LowUserEngagementModel : public ModelProvider {
 public:
  LowUserEngagementModel();
  ~LowUserEngagementModel() override = default;

  // Disallow copy/assign.
  LowUserEngagementModel(LowUserEngagementModel&) = delete;
  LowUserEngagementModel& operator=(LowUserEngagementModel&) = delete;

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_DEFAULT_MODEL_LOW_USER_ENGAGEMENT_MODEL_H_
