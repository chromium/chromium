// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_

#include <memory>
#include <string_view>
#include <vector>

#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace content {
class BrowserContext;
}

namespace segmentation_platform {
namespace home_modules {
class HomeModulesCardRegistry;
}

struct Config;

// Returns a Config created from the finch feature params.
std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig(
    content::BrowserContext* context,
    home_modules::HomeModulesCardRegistry* home_modules_card_registry);

// Finds a list of configs from experiments and appends to `out_configs`.
// Public for testing.
void AppendConfigsFromExperiments(
    std::vector<std::unique_ptr<Config>>& out_configs);

// Implementation of FieldTrialRegister that uses synthetic field trials to
// record segmentation groups.
class FieldTrialRegisterImpl : public FieldTrialRegister {
 public:
  FieldTrialRegisterImpl();
  ~FieldTrialRegisterImpl() override;
  FieldTrialRegisterImpl(const FieldTrialRegisterImpl&) = delete;
  FieldTrialRegisterImpl& operator=(const FieldTrialRegisterImpl&) = delete;

  // FieldTrialRegister:
  void RegisterFieldTrial(std::string_view trial_name,
                          std::string_view group_name) override;

  void RegisterSubsegmentFieldTrialIfNeeded(std::string_view trial_name,
                                            proto::SegmentId segment_id,
                                            int subsegment_rank) override;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_
