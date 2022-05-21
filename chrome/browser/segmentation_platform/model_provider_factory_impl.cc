// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/model_provider_factory_impl.h"

#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {
namespace {

class DummyModelProvider : public ModelProvider {
 public:
  DummyModelProvider()
      : ModelProvider(proto::SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {}
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override {}

  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override {
    std::move(callback).Run(absl::nullopt);
  }

  bool ModelAvailable() override { return false; }
};

}  // namespace

ModelProviderFactoryImpl::ModelProviderFactoryImpl(
    optimization_guide::OptimizationGuideModelProvider*
        optimization_guide_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : optimization_guide_provider_(optimization_guide_provider),
      background_task_runner_(background_task_runner) {}

ModelProviderFactoryImpl::~ModelProviderFactoryImpl() = default;

std::unique_ptr<ModelProvider> ModelProviderFactoryImpl::CreateProvider(
    proto::SegmentId segment_id) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (!optimization_guide_provider_) {
    // Optimization guide may not be available in some tests,
    return std::make_unique<DummyModelProvider>();
  }
  return std::make_unique<OptimizationGuideSegmentationModelProvider>(
      optimization_guide_provider_, background_task_runner_, segment_id);
#else
  return std::make_unique<DummyModelProvider>();
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

std::unique_ptr<ModelProvider> ModelProviderFactoryImpl::CreateDefaultProvider(
    proto::SegmentId segment_id) {
  return DefaultModelsRegister::GetInstance().GetModelProvider(segment_id);
}

}  // namespace segmentation_platform
