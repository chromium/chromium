// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/task_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/record_replay/core/browser/task_service.h"
#include "components/record_replay/core/common/record_replay_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {
namespace {

class TaskServiceFactoryTest : public ChromeRenderViewHostTestHarness {
 public:
  TaskServiceFactoryTest() = default;
  ~TaskServiceFactoryTest() override = default;
};

TEST_F(TaskServiceFactoryTest, ReturnsInstanceWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kRecordReplayBase);

  TaskService* service = TaskServiceFactory::GetForProfile(profile());
  EXPECT_NE(service, nullptr);
}

TEST_F(TaskServiceFactoryTest, ReturnsNullptrWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kRecordReplayBase);

  TaskService* service = TaskServiceFactory::GetForProfile(profile());
  EXPECT_EQ(service, nullptr);
}

}  // namespace
}  // namespace record_replay
