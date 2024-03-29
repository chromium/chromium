// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action/share_action.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/share_action/share_action_cache.h"
#include "chrome/browser/sharesheet/sharesheet_test_util.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharesheet {

class ShareActionTest : public ::testing::Test {
 public:
  // Test:
  void SetUp() override {
    auto profile = std::make_unique<TestingProfile>();
    share_action_cache_ = std::make_unique<ShareActionCache>(profile.get());
  }

  ShareActionCache* share_action_cache() { return share_action_cache_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<ShareActionCache> share_action_cache_;
};

TEST_F(ShareActionTest, ShareActionCacheGetAllActions) {
  auto& share_actions = share_action_cache()->GetShareActions();
  // We don't expect Nearby Share because it's not supported in this context.
  EXPECT_EQ(share_actions.size(), 2u);
  // We can check like this because ordering of actions in |share_action_cache_|
  // is predetermined.
  EXPECT_EQ(share_actions[0]->GetActionType(), ShareActionType::kDriveShare);
  EXPECT_EQ(share_actions[1]->GetActionType(),
            ShareActionType::kCopyToClipboardShare);
}

TEST_F(ShareActionTest, ShareActionCacheGetActionFromType) {
  auto* share_action =
      share_action_cache()->GetActionFromType(ShareActionType::kDriveShare);
  EXPECT_EQ(share_action->GetActionType(), ShareActionType::kDriveShare);

  share_action = share_action_cache()->GetActionFromType(
      ShareActionType::kCopyToClipboardShare);
  EXPECT_EQ(share_action->GetActionType(),
            ShareActionType::kCopyToClipboardShare);
}

TEST_F(ShareActionTest, ShareActionCacheGetVectorIconFromType) {
  auto* share_action =
      share_action_cache()->GetActionFromType(ShareActionType::kDriveShare);
  EXPECT_EQ(share_action->GetActionType(), ShareActionType::kDriveShare);

  auto* vector_icon =
      share_action_cache()->GetVectorIconFromType(ShareActionType::kDriveShare);
  EXPECT_TRUE(vector_icon);
  EXPECT_EQ(&share_action->GetActionIcon(), vector_icon);
}

TEST_F(ShareActionTest, ShareActionCacheHasVisibleActions) {
  EXPECT_TRUE(share_action_cache()->HasVisibleActions(
      CreateValidTextIntent(), /*contains_hosted_document=*/false));
  // False due to invalid intent.
  EXPECT_FALSE(share_action_cache()->HasVisibleActions(
      CreateInvalidIntent(), /*contains_hosted_document=*/false));
  // False if the intent contains a hosted document that is not a drive intent.
  EXPECT_FALSE(share_action_cache()->HasVisibleActions(
      CreateValidTextIntent(), /*contains_hosted_document=*/true));
  // True as a drive intent means drive_share_action is visible.
  EXPECT_TRUE(share_action_cache()->HasVisibleActions(
      CreateDriveIntent(), /*contains_hosted_document=*/true));
}

}  // namespace sharesheet
