// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"

#include <memory>

#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class ContextualTasksServiceFactoryTest : public testing::Test {
 protected:
  ContextualTasksServiceFactoryTest() = default;
  ~ContextualTasksServiceFactoryTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ContextualTasksServiceFactoryTest, UsesRealService) {
  profile_ = TestingProfile::Builder().Build();

  ContextualTasksService* service =
      ContextualTasksServiceFactory::GetForProfile(profile_.get());
  EXPECT_NE(nullptr, service);
}

TEST_F(ContextualTasksServiceFactoryTest, UsesNullInIncognito) {
  profile_ = TestingProfile::Builder().Build();

  Profile* otr_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  ContextualTasksService* service =
      ContextualTasksServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(nullptr, service);
}

}  // namespace contextual_tasks
