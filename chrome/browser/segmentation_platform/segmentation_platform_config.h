// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_

#include <memory>
#include <vector>

#include "components/optimization_guide/proto/models.pb.h"

namespace segmentation_platform {
struct Config;
class ModelProvider;

// Returns a Config created from the finch feature params.
std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig();

// Returns a default model provider for the `target`.
std::unique_ptr<ModelProvider> GetSegmentationDefaultModelProvider(
    optimization_guide::proto::OptimizationTarget target);

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_
