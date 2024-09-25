// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/intent.h"

#include <string>

#include "ash/components/arc/app/arc_app_constants.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kTestPackage[] = "com.test.app";
constexpr char kTestActivity[] = "com.test.app.some.activity";
constexpr char kTestShelfGroupId[] = "some_shelf_group";
constexpr char kIntentWithShelfGroupId[] =
    "#Intent;launchFlags=0x18001000;"
    "component=com.test.app/com.test.app.some.activity;"
    "S.org.chromium.arc.shelf_group_id=some_shelf_group;end";

std::string GetPlayStoreInitialLaunchIntent() {
  return GetLaunchIntent(kPlayStorePackage, kPlayStoreActivity,
                         {kInitialStartParam});
}

}  // namespace

using ArcIntentTest = testing::Test;

TEST_F(ArcIntentTest, LaunchIntent) {
  const std::string launch_intent = GetPlayStoreInitialLaunchIntent();

  auto intent1 = Intent::Get(launch_intent);
  ASSERT_TRUE(intent1);
  EXPECT_EQ(intent1->action(), "android.intent.action.MAIN");
  EXPECT_EQ(intent1->category(), "android.intent.category.LAUNCHER");
  EXPECT_EQ(intent1->package_name(), kPlayStorePackage);
  EXPECT_EQ(intent1->activity(), kPlayStoreActivity);
  EXPECT_EQ(intent1->launch_flags(),
            Intent::FLAG_ACTIVITY_NEW_TASK |
                Intent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
  ASSERT_EQ(intent1->extra_params().size(), 1U);
  EXPECT_TRUE(intent1->HasExtraParam(kInitialStartParam));

  auto intent2 = Intent::Get(kIntentWithShelfGroupId);
  ASSERT_TRUE(intent2);
  EXPECT_EQ(intent2->action(), "");
  EXPECT_EQ(intent2->category(), "");
  EXPECT_EQ(intent2->package_name(), kTestPackage);
  EXPECT_EQ(intent2->activity(), kTestActivity);
  EXPECT_EQ(intent2->launch_flags(), Intent::FLAG_ACTIVITY_NEW_TASK |
                                         Intent::FLAG_RECEIVER_NO_ABORT |
                                         Intent::FLAG_ACTIVITY_LAUNCH_ADJACENT);
  ASSERT_EQ(intent2->extra_params().size(), 1U);
  EXPECT_TRUE(intent2->HasExtraParam(
      "S.org.chromium.arc.shelf_group_id=some_shelf_group"));
}

TEST_F(ArcIntentTest, AppendLaunchIntent) {
  const std::string param1 =
      CreateIntentTicksExtraParam(kRequestDeferredStartTimeParamKey,
                                  base::TimeTicks() + base::Milliseconds(100));
  const std::string param2 = CreateIntentTicksExtraParam(
      kRequestStartTimeParamKey, base::TimeTicks() + base::Milliseconds(200));
  const std::string launch_intent =
      AppendLaunchIntent(GetPlayStoreInitialLaunchIntent(), {param1, param2});

  auto intent = Intent::Get(launch_intent);
  ASSERT_TRUE(intent);
  EXPECT_EQ(intent->action(), "android.intent.action.MAIN");
  EXPECT_EQ(intent->category(), "android.intent.category.LAUNCHER");
  EXPECT_EQ(intent->package_name(), kPlayStorePackage);
  EXPECT_EQ(intent->activity(), kPlayStoreActivity);
  EXPECT_EQ(intent->launch_flags(),
            Intent::FLAG_ACTIVITY_NEW_TASK |
                Intent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
  ASSERT_EQ(intent->extra_params().size(), 3U);
  EXPECT_TRUE(intent->HasExtraParam(kInitialStartParam));
  std::string deferred_time;
  EXPECT_TRUE(intent->GetExtraParamValue(kRequestDeferredStartTimeParamKey,
                                         &deferred_time));
  EXPECT_EQ("100", deferred_time);
  std::string start_time;
  EXPECT_TRUE(
      intent->GetExtraParamValue(kRequestStartTimeParamKey, &start_time));
  EXPECT_EQ("200", start_time);
}

TEST_F(ArcIntentTest, CreateIntentTicksExtraParam) {
  EXPECT_EQ("S.org.chromium.arc.request.deferred.start=12345",
            CreateIntentTicksExtraParam(
                "S.org.chromium.arc.request.deferred.start",
                base::TimeTicks() + base::Milliseconds(12345)));
}

TEST_F(ArcIntentTest, ShelfGroupId) {
  const std::string intent_with_shelf_group_id(kIntentWithShelfGroupId);
  const std::string shelf_app_id =
      ArcAppListPrefs::GetAppId(kTestPackage, kTestActivity);
  ArcAppShelfId shelf_id1 = ArcAppShelfId::FromIntentAndAppId(
      intent_with_shelf_group_id, shelf_app_id);
  EXPECT_TRUE(shelf_id1.has_shelf_group_id());
  EXPECT_EQ(shelf_id1.shelf_group_id(), kTestShelfGroupId);
  EXPECT_EQ(shelf_id1.app_id(), shelf_app_id);

  ArcAppShelfId shelf_id2 = ArcAppShelfId::FromIntentAndAppId(
      GetPlayStoreInitialLaunchIntent(), kPlayStoreAppId);
  EXPECT_FALSE(shelf_id2.has_shelf_group_id());
  EXPECT_EQ(shelf_id2.app_id(), kPlayStoreAppId);
}

TEST_F(ArcIntentTest, EmptyExtraParams) {
  auto intent =
      Intent::Get(GetLaunchIntent(kPlayStorePackage, kPlayStoreActivity, {}));
  ASSERT_TRUE(intent);
  EXPECT_TRUE(intent->extra_params().empty());
}

TEST_F(ArcIntentTest, WrongIntents) {
  // ;; is bad part here.
  EXPECT_FALSE(
      Intent::Get("#Intent;action=android.intent.action.MAIN;category=android."
                  "intent.category.LAUNCHER;launchFlags=0x10200000;component="
                  "com.android.vending/.AssetBrowserActivity;;end"));
  EXPECT_TRUE(
      Intent::Get("#Intent;action=android.intent.action.MAIN;category=android."
                  "intent.category.LAUNCHER;launchFlags=0x10200000;component="
                  "com.android.vending/.AssetBrowserActivity;end"));
}

}  // namespace arc
