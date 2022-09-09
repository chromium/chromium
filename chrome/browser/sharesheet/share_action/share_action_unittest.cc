// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action/share_action.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/share_action/share_action_cache.h"
#include "chrome/browser/sharesheet/sharesheet_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

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
  EXPECT_EQ(share_actions[0]->GetActionName(),
            l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL));
  EXPECT_EQ(share_actions[1]->GetActionName(),
            l10n_util::GetStringUTF16(
                IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL));
}

TEST_F(ShareActionTest, ShareActionCacheGetActionFromName) {
  auto name = l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL);
  l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL);
  auto* share_action = share_action_cache()->GetActionFromName(name);
  EXPECT_EQ(share_action->GetActionName(), name);

  name = l10n_util::GetStringUTF16(
      IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL);
  share_action = share_action_cache()->GetActionFromName(name);
  EXPECT_EQ(share_action->GetActionName(), name);
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
