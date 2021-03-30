// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class IntentUtilsTest : public testing::Test {
 protected:
  arc::mojom::IntentInfoPtr CreateArcIntent() {
    arc::mojom::IntentInfoPtr arc_intent = arc::mojom::IntentInfo::New();
    arc_intent->action = "android.intent.action.PROCESS_TEXT";
    std::vector<std::string> categories = {"text"};
    arc_intent->categories = categories;
    arc_intent->data = "/tmp";
    arc_intent->type = "text/plain";
    arc_intent->ui_bypassed = false;
    base::flat_map<std::string, std::string> extras = {
        {"android.intent.action.PROCESS_TEXT", "arc_apps"}};
    arc_intent->extras = extras;
    return arc_intent;
  }

  arc::mojom::ActivityNamePtr CreateActivity() {
    arc::mojom::ActivityNamePtr arc_activity = arc::mojom::ActivityName::New();
    arc_activity->package_name = "com.google.android.apps.translate";
    arc_activity->activity_name =
        "com.google.android.apps.translate.TranslateActivity";
    return arc_activity;
  }

  bool IsEqual(arc::mojom::IntentInfoPtr src_intent,
               arc::mojom::IntentInfoPtr dst_intent) {
    if (!src_intent && !dst_intent) {
      return true;
    }

    if (!src_intent || !dst_intent) {
      return false;
    }

    if (src_intent->action != dst_intent->action) {
      return false;
    }

    if (src_intent->categories != dst_intent->categories) {
      return false;
    }

    if (src_intent->data != dst_intent->data) {
      return false;
    }

    if (src_intent->ui_bypassed != dst_intent->ui_bypassed) {
      return false;
    }

    if (src_intent->extras != dst_intent->extras) {
      return false;
    }

    return true;
  }

  bool IsEqual(arc::mojom::ActivityNamePtr src_activity,
               arc::mojom::ActivityNamePtr dst_activity) {
    if (!src_activity && !dst_activity) {
      return true;
    }

    if (!src_activity || !dst_activity) {
      return false;
    }

    if (src_activity->activity_name != dst_activity->activity_name) {
      return false;
    }

    return true;
  }
};

TEST_F(IntentUtilsTest, CreateIntentForArcIntentAndActivity) {
  arc::mojom::IntentInfoPtr arc_intent = CreateArcIntent();
  arc::mojom::ActivityNamePtr src_activity = CreateActivity();
  apps::mojom::IntentPtr intent =
      apps_util::CreateIntentForArcIntentAndActivity(arc_intent.Clone(),
                                                     src_activity.Clone());

  arc::mojom::ActivityNamePtr dst_activity = arc::mojom::ActivityName::New();
  if (intent->activity_name.has_value() &&
      !intent->activity_name.value().empty()) {
    dst_activity->activity_name = intent->activity_name.value();
  }

  EXPECT_TRUE(
      IsEqual(std::move(arc_intent), apps_util::CreateArcIntent(intent)));
  EXPECT_TRUE(IsEqual(std::move(src_activity), std::move(dst_activity)));
}

TEST_F(IntentUtilsTest, CreateIntentForActivity) {
  const std::string& activity_name = "com.android.vending.AssetBrowserActivity";
  const std::string& start_type = "initialStart";
  const std::string& category = "android.intent.category.LAUNCHER";
  apps::mojom::IntentPtr intent =
      apps_util::CreateIntentForActivity(activity_name, start_type, category);
  arc::mojom::IntentInfoPtr arc_intent = apps_util::CreateArcIntent(intent);

  ASSERT_TRUE(intent);
  ASSERT_TRUE(arc_intent);

  EXPECT_EQ(arc::kIntentActionMain, arc_intent->action);

  base::flat_map<std::string, std::string> extras;
  extras.insert(std::make_pair("org.chromium.arc.start_type", start_type));
  EXPECT_TRUE(arc_intent->extras.has_value());
  EXPECT_EQ(extras, arc_intent->extras);

  EXPECT_TRUE(arc_intent->categories.has_value());
  EXPECT_EQ(category, arc_intent->categories.value()[0]);

  arc_intent->extras = apps_util::CreateArcIntentExtras(intent);
  EXPECT_TRUE(intent->activity_name.has_value());
  EXPECT_EQ(activity_name, intent->activity_name.value());
}
