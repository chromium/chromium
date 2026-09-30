// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_gating_service.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/devtools/devtools_navigation_gating_rule_manager.h"
#include "components/origin_gating/core/checker_id.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class DevToolsNavigationGatingServiceTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<origin_gating::OriginGatingService> origin_gating_service_{
      origin_gating::OriginGatingService::CreateForTesting()};
};

TEST_F(DevToolsNavigationGatingServiceTest, RegistersCheckerUsingRuleManager) {
  auto rule_manager = DevToolsNavigationGatingRuleManager::CreateForTesting(
      R"({"blocklist": ["https://blocked.com"]})");
  auto service = DevToolsNavigationGatingService::CreateForTesting(
      *origin_gating_service_, *rule_manager);

  base::test::TestFuture<bool> future;
  service->IsNavigationAllowed(GURL("https://blocked.com"),
                               future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(DevToolsNavigationGatingServiceTest, DestructionUnregistersChecker) {
  auto rule_manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting("{}");
  auto service = DevToolsNavigationGatingService::CreateForTesting(
      *origin_gating_service_, *rule_manager);
  // The first checker registered with a fresh service gets the first ID.
  origin_gating::CheckerId id =
      origin_gating::CheckerId::Generator().GenerateNextId();
  EXPECT_NE(origin_gating_service_->GetChecker(id), nullptr);

  service.reset();
  EXPECT_EQ(origin_gating_service_->GetChecker(id), nullptr);
}

}  // namespace
