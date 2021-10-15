// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action/share_action.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/share_action/share_action_cache.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace {

const std::u16string kCopyToClipboardName = u"Copy to clipboard";
const char kTestUrl[] = "https://fake-url.com/fake";

apps::mojom::IntentPtr CreateValidDefaultIntent() {
  return apps_util::CreateShareIntentFromText("text", "title");
}

apps::mojom::IntentPtr CreateInvalidIntent() {
  auto intent = apps::mojom::Intent::New();
  intent->action = apps_util::kIntentActionSend;
  return intent;
}

apps::mojom::IntentPtr CreateDriveIntent() {
  return apps_util::CreateShareIntentFromDriveFile(GURL(kTestUrl), "image/",
                                                   GURL(kTestUrl), false);
}

}  // namespace

namespace sharesheet {

class ShareActionTest : public ::testing::Test {
 public:
  ShareActionTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSharesheetCopyToClipboard);
  }

  // Test:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    share_action_cache_ = std::make_unique<ShareActionCache>(profile_.get());
  }

  ShareActionCache* share_action_cache() { return share_action_cache_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

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
  EXPECT_EQ(share_actions[1]->GetActionName(), kCopyToClipboardName);
}

TEST_F(ShareActionTest, ShareActionCacheGetActionFromName) {
  auto* share_action = share_action_cache()->GetActionFromName(
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL));
  EXPECT_EQ(share_action->GetActionName(),
            l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL));

  share_action = share_action_cache()->GetActionFromName(kCopyToClipboardName);
  EXPECT_EQ(share_action->GetActionName(), kCopyToClipboardName);
}

TEST_F(ShareActionTest, ShareActionCacheHasVisibleActions) {
  EXPECT_TRUE(share_action_cache()->HasVisibleActions(
      CreateValidDefaultIntent(), /*contains_hosted_document=*/false));
  // False due to invalid intent.
  EXPECT_FALSE(share_action_cache()->HasVisibleActions(
      CreateInvalidIntent(), /*contains_hosted_document=*/false));
  // False if the intent contains a hosted document that is not a drive intent.
  EXPECT_FALSE(share_action_cache()->HasVisibleActions(
      CreateValidDefaultIntent(), /*contains_hosted_document=*/true));
  // True as a drive intent means drive_share_action is visible.
  EXPECT_TRUE(share_action_cache()->HasVisibleActions(
      CreateDriveIntent(), /*contains_hosted_document=*/true));
}

}  // namespace sharesheet
