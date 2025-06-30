// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <optional>

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

class ActorKeyedServiceTest : public testing::Test {
 public:
  ActorKeyedServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ActorKeyedServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager()->CreateTestingProfile("profile");
  }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

// Adds a task to ActorKeyedService
TEST_F(ActorKeyedServiceTest, AddActiveTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  actor_service->AddActiveTask(std::make_unique<ActorTask>());
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 1u);
  EXPECT_EQ(actor_service->GetActiveTasks().begin()->second->GetState(),
            ActorTask::State::kCreated);
}

// Stops a task.
TEST_F(ActorKeyedServiceTest, StopActiveTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->AddActiveTask(std::make_unique<ActorTask>());
  actor_service->StopTask(id);
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 0u);
  ASSERT_EQ(actor_service->GetInactiveTasks().size(), 1u);
  EXPECT_EQ(actor_service->GetInactiveTasks().begin()->second->GetState(),
            ActorTask::State::kFinished);
  EXPECT_EQ(actor_service->GetInactiveTasks().begin()->second->GetEndTime(),
            base::Time::Now());
}

}  // namespace

}  // namespace actor
