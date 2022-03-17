// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_PROVIDER_FACTORY_IMPL_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_PROVIDER_FACTORY_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}

namespace segmentation_platform {

class ModelProviderFactoryImpl : public ModelProviderFactory {
 public:
  ModelProviderFactoryImpl(
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  ~ModelProviderFactoryImpl() override;

  ModelProviderFactoryImpl(ModelProviderFactoryImpl&) = delete;
  ModelProviderFactoryImpl& operator=(ModelProviderFactoryImpl&) = delete;

  // ModelProviderFactory impl:
  std::unique_ptr<ModelProvider> CreateProvider(
      optimization_guide::proto::OptimizationTarget optimization_target)
      override;

 private:
  raw_ptr<optimization_guide::OptimizationGuideModelProvider>
      optimization_guide_provider_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_PROVIDER_FACTORY_IMPL_H_
