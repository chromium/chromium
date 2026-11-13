// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"

#include <memory>

#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicInstanceCoordinatorImplTest : public testing::Test {
 public:
  GlicInstanceCoordinatorImplTest() = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<GlicEnabling> enabling_ =
      GlicEnabling::CreateForTesting(&profile_, nullptr);
};

TEST_F(GlicInstanceCoordinatorImplTest, ShutdownUnregistersMemoryConsumer) {
  base::TestMemoryConsumerRegistry registry;

  auto coordinator = std::make_unique<GlicInstanceCoordinatorImpl>(
      &profile_,
      /*identity_manager=*/nullptr,
      /*service=*/nullptr, enabling_.get(),
      /*contextual_cueing_service=*/nullptr);
  EXPECT_EQ(registry.size(), 1u);

  coordinator->Shutdown();
  EXPECT_EQ(registry.size(), 0u);
}

}  // namespace glic
