// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/model_provider_factory_impl.h"

#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class ModelProviderFactoryImplTest : public testing::Test {
 public:
  ModelProviderFactoryImplTest() = default;
  ~ModelProviderFactoryImplTest() override = default;

  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    provider_factory_ = std::make_unique<ModelProviderFactoryImpl>(
        model_provider_.get(), task_runner_);
  }

  void TearDown() override {
    task_runner_->RunPendingTasks();
    provider_factory_.reset();
    model_provider_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;

  std::unique_ptr<ModelProviderFactoryImpl> provider_factory_;
};

TEST_F(ModelProviderFactoryImplTest, ProviderCreated) {
  EXPECT_TRUE(provider_factory_->CreateProvider(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_VOICE));
  EXPECT_TRUE(provider_factory_->CreateProvider(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_SHARE));
}

}  // namespace segmentation_platform
