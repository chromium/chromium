// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_DEFAULT_MODEL_CHROME_START_MODEL_ANDROID_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_DEFAULT_MODEL_CHROME_START_MODEL_ANDROID_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

struct Config;

// Segmentation Chrome Start model provider. Provides a default model and
// metadata for the chrome start optimization target.
class ChromeStartModel : public DefaultModelProvider {
 public:
  ChromeStartModel();
  ~ChromeStartModel() override = default;

  // Disallow copy/assign.
  ChromeStartModel(const ChromeStartModel&) = delete;
  ChromeStartModel& operator=(const ChromeStartModel&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;

  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_DEFAULT_MODEL_CHROME_START_MODEL_ANDROID_H_
