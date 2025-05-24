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
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
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

// Adds a task to ActorKeyedService, and then checks that fetching the
// ActorKeyedService from a profile shows that task.
TEST_F(ActorKeyedServiceTest, AddTask) {
  ActorKeyedService::Get(profile())->AddTask(std::make_unique<ActorTask>());
  ASSERT_EQ(ActorKeyedService::Get(profile())->GetTasks().size(), 1u);
  EXPECT_EQ(ActorKeyedService::Get(profile())->GetTasks()[0]->GetState(),
            ActorTask::State::kCreated);
}

}  // namespace

}  // namespace actor
