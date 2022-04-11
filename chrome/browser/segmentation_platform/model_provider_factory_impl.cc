// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/model_provider_factory_impl.h"

#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_provider.h"

namespace segmentation_platform {
namespace {

#if !BUILDFLAG(BUILD_WITH_TFLITE_LIB)
class DummyModelProvider : public ModelProvider {
 public:
  DummyModelProvider()
      : ModelProvider(optimization_guide::proto::OptimizationTarget::
                          OPTIMIZATION_TARGET_UNKNOWN) {}
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override {}
  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override {}
  bool ModelAvailable() override { return false; }
};
#endif

}  // namespace

ModelProviderFactoryImpl::ModelProviderFactoryImpl(
    optimization_guide::OptimizationGuideModelProvider*
        optimization_guide_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : optimization_guide_provider_(optimization_guide_provider),
      background_task_runner_(background_task_runner) {}

ModelProviderFactoryImpl::~ModelProviderFactoryImpl() = default;

std::unique_ptr<ModelProvider> ModelProviderFactoryImpl::CreateProvider(
    optimization_guide::proto::OptimizationTarget optimization_target) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return std::make_unique<OptimizationGuideSegmentationModelProvider>(
      optimization_guide_provider_, background_task_runner_,
      optimization_target);
#else
  return std::make_unique<DummyModelProvider>();
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

std::unique_ptr<ModelProvider> ModelProviderFactoryImpl::CreateDefaultProvider(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  return GetSegmentationDefaultModelProvider(optimization_target);
}

}  // namespace segmentation_platform
