// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {
struct Config;
class ModelProvider;

// Returns a Config created from the finch feature params.
std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig();

// Returns a default model provider for the `target`.
class DefaultModelsRegister {
 public:
  static DefaultModelsRegister& GetInstance();

  ~DefaultModelsRegister();
  DefaultModelsRegister(const DefaultModelsRegister& client) = delete;
  DefaultModelsRegister& operator=(const DefaultModelsRegister& client) =
      delete;

  std::unique_ptr<ModelProvider> GetModelProvider(proto::SegmentId target);

  void SetModelForTesting(proto::SegmentId target,
                          std::unique_ptr<ModelProvider>);

 private:
  friend class base::NoDestructor<DefaultModelsRegister>;

  DefaultModelsRegister();

  std::map<proto::SegmentId, std::unique_ptr<ModelProvider>> providers_;
};

// Implementation of FieldTrialRegister that uses synthetic field trials to
// record segmentation groups.
class FieldTrialRegisterImpl : public FieldTrialRegister {
 public:
  FieldTrialRegisterImpl();
  ~FieldTrialRegisterImpl() override;
  FieldTrialRegisterImpl(const FieldTrialRegisterImpl&) = delete;
  FieldTrialRegisterImpl& operator=(const FieldTrialRegisterImpl&) = delete;

  // FieldTrialRegister:
  void RegisterFieldTrial(base::StringPiece trial_name,
                          base::StringPiece group_name) override;

  void RegisterSubsegmentFieldTrialIfNeeded(base::StringPiece trial_name,
                                            proto::SegmentId segment_id,
                                            int subsegment_rank) override;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_
