// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using apps::mojom::Condition;
using apps::mojom::ConditionType;
using apps::mojom::IntentFilterPtr;
using apps::mojom::PatternMatchType;

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

  std::string intent_str =
      apps_util::CreateLaunchIntent("com.android.vending", intent);
  EXPECT_TRUE(intent_str.empty());

  arc::mojom::ActivityNamePtr dst_activity = arc::mojom::ActivityName::New();
  if (intent->activity_name.has_value() &&
      !intent->activity_name.value().empty()) {
    dst_activity->activity_name = intent->activity_name.value();
  }

  EXPECT_TRUE(IsEqual(std::move(arc_intent),
                      apps_util::ConvertAppServiceToArcIntent(intent)));
  EXPECT_TRUE(IsEqual(std::move(src_activity), std::move(dst_activity)));
}

TEST_F(IntentUtilsTest, CreateIntentForActivity) {
  const std::string& activity_name = "com.android.vending.AssetBrowserActivity";
  const std::string& start_type = "initialStart";
  const std::string& category = "android.intent.category.LAUNCHER";
  apps::mojom::IntentPtr intent =
      apps_util::CreateIntentForActivity(activity_name, start_type, category);
  arc::mojom::IntentInfoPtr arc_intent =
      apps_util::ConvertAppServiceToArcIntent(intent);

  ASSERT_TRUE(intent);
  ASSERT_TRUE(arc_intent);

  std::string intent_str =
      "#Intent;action=android.intent.action.MAIN;category=android.intent."
      "category.LAUNCHER;launchFlags=0x10200000;component=com.android.vending/"
      ".AssetBrowserActivity;S.org.chromium.arc.start_type=initialStart;end";
  EXPECT_EQ(intent_str,
            apps_util::CreateLaunchIntent("com.android.vending", intent));

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

TEST_F(IntentUtilsTest, CreateShareIntentFromText) {
  apps::mojom::IntentPtr intent =
      apps_util::CreateShareIntentFromText("text", "title");
  std::string intent_str =
      "#Intent;action=android.intent.action.SEND;launchFlags=0x10200000;"
      "component=com.android.vending/;type=text/"
      "plain;S.android.intent.extra.TEXT=text;S.android.intent.extra.SUBJECT="
      "title;end";
  EXPECT_EQ(intent_str,
            apps_util::CreateLaunchIntent("com.android.vending", intent));
}

TEST_F(IntentUtilsTest, CreateWebAppIntentFilters_ShortcutApp_NoUrlFilter) {
  auto web_app = web_app::test::CreateMinimalWebApp();
  // Ensure scope is empty, as in a "Create Shortcut" app.
  web_app->SetScope(GURL());

  std::vector<IntentFilterPtr> filters =
      apps_util::CreateWebAppIntentFilters(*web_app.get());

  EXPECT_EQ(filters.size(), 0);
}

TEST_F(IntentUtilsTest, CreateWebAppIntentFilters_WebApp_HasUrlFilter) {
  auto web_app = web_app::test::CreateMinimalWebApp();
  DCHECK(web_app->start_url().is_valid());
  GURL scope = web_app->start_url().GetWithoutFilename();
  web_app->SetScope(scope);

  std::vector<IntentFilterPtr> filters =
      apps_util::CreateWebAppIntentFilters(*web_app.get());

  ASSERT_EQ(filters.size(), 1);
  IntentFilterPtr& filter = filters[0];
  EXPECT_FALSE(filter->activity_name.has_value());
  EXPECT_FALSE(filter->activity_label.has_value());
  ASSERT_EQ(filter->conditions.size(), 4U);

  {
    const Condition& condition = *filter->conditions[0];
    EXPECT_EQ(condition.condition_type, ConditionType::kAction);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value,
              apps_util::kIntentActionView);
  }

  {
    const Condition& condition = *filter->conditions[1];
    EXPECT_EQ(condition.condition_type, ConditionType::kScheme);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, scope.scheme());
  }

  {
    const Condition& condition = *filter->conditions[2];
    EXPECT_EQ(condition.condition_type, ConditionType::kHost);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, scope.host());
  }

  {
    const Condition& condition = *filter->conditions[3];
    EXPECT_EQ(condition.condition_type, ConditionType::kPattern);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kPrefix);
    EXPECT_EQ(condition.condition_values[0]->value, scope.path());
  }

  EXPECT_TRUE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(web_app->start_url()), filter));

  EXPECT_FALSE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(GURL("https://bar.com")), filter));
}

TEST_F(IntentUtilsTest, CreateWebAppIntentFilters_FileHandlers) {
  auto web_app = web_app::test::CreateMinimalWebApp();
  DCHECK(web_app->start_url().is_valid());
  GURL scope = web_app->start_url().GetWithoutFilename();
  web_app->SetScope(scope);

  apps::FileHandler::AcceptEntry accept_entry;
  accept_entry.mime_type = "text/plain";
  accept_entry.file_extensions.insert(".txt");
  apps::FileHandler file_handler;
  file_handler.action = GURL("https://example.com/path/handler.html");
  file_handler.accept.push_back(std::move(accept_entry));
  apps::FileHandlers file_handlers;
  file_handlers.push_back(std::move(file_handler));
  web_app->SetFileHandlers(file_handlers);

  std::vector<IntentFilterPtr> filters =
      apps_util::CreateWebAppIntentFilters(*web_app.get());

  ASSERT_EQ(filters.size(), 3);
  // 1st filter is URL filter.

  // Mime type filter - View action
  const IntentFilterPtr& mime_filter = filters[1];
  ASSERT_EQ(mime_filter->conditions.size(), 2);
  const Condition& view_cond = *mime_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // Mime type filter - mime type match
  const Condition& mime_cond = *mime_filter->conditions[1];
  EXPECT_EQ(mime_cond.condition_type, ConditionType::kMimeType);
  ASSERT_EQ(mime_cond.condition_values.size(), 1);
  EXPECT_EQ(mime_cond.condition_values[0]->match_type,
            PatternMatchType::kMimeType);
  EXPECT_EQ(mime_cond.condition_values[0]->value, "text/plain");

  // File extension filter - View action
  const IntentFilterPtr& file_ext_filter = filters[2];
  ASSERT_EQ(file_ext_filter->conditions.size(), 2);
  const Condition& view_cond2 = *file_ext_filter->conditions[0];
  EXPECT_EQ(view_cond2.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond2.condition_values.size(), 1);
  EXPECT_EQ(view_cond2.condition_values[0]->value,
            apps_util::kIntentActionView);

  // File extension filter - file extension match
  const Condition& file_ext_cond = *file_ext_filter->conditions[1];
  EXPECT_EQ(file_ext_cond.condition_type, ConditionType::kFileExtension);
  ASSERT_EQ(file_ext_cond.condition_values.size(), 1);
  EXPECT_EQ(file_ext_cond.condition_values[0]->match_type,
            PatternMatchType::kFileExtension);
  EXPECT_EQ(file_ext_cond.condition_values[0]->value, ".txt");
}

TEST_F(IntentUtilsTest, CreateWebAppIntentFilters_NoteTakingApp) {
  base::test::ScopedFeatureList features{blink::features::kWebAppNoteTaking};
  auto web_app = web_app::test::CreateMinimalWebApp();
  DCHECK(web_app->start_url().is_valid());
  GURL scope = web_app->start_url().GetWithoutFilename();
  web_app->SetScope(scope);
  GURL new_note_url = scope.Resolve("/new_note.html");
  web_app->SetNoteTakingNewNoteUrl(new_note_url);

  std::vector<IntentFilterPtr> filters =
      apps_util::CreateWebAppIntentFilters(*web_app.get());

  ASSERT_EQ(filters.size(), 2);

  // 1st filter is URL filter.
  EXPECT_TRUE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(scope), filters[0]));

  // 2nd filter is note-taking filter.
  ASSERT_EQ(filters[1]->conditions.size(), 1);
  const Condition& condition = *filters[1]->conditions[0];
  EXPECT_EQ(condition.condition_type, ConditionType::kAction);
  ASSERT_EQ(condition.condition_values.size(), 1);
  EXPECT_EQ(condition.condition_values[0]->value,
            apps_util::kIntentActionCreateNote);
  EXPECT_TRUE(apps_util::IntentMatchesFilter(
      apps_util::CreateCreateNoteIntent(), filters[1]));
}

TEST_F(IntentUtilsTest, CreateShareIntentFromFiles_GetFileUrls) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  constexpr char kTestData[] = "testing1234";
  constexpr char kTestText[] = "text1234";
  constexpr char kShareTitle[] = "title1234";
  std::vector<base::FilePath> file_paths;
  std::vector<std::string> mime_types;

  base::FilePath test_file_path =
      scoped_temp_dir.GetPath().AppendASCII("test.txt");
  ASSERT_EQ(static_cast<int>(base::size(kTestData)),
            base::WriteFile(test_file_path, kTestData, base::size(kTestData)));
  file_paths.push_back(test_file_path);
  mime_types.push_back(".txt");

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      absl::nullopt, file_paths, mime_types, kTestText, kShareTitle);
  const std::string intent_str =
      "#Intent;action=android.intent.action.SEND;launchFlags=0x10200000;"
      "component=com.android.vending/;type=*/*;"
      "S.android.intent.extra.TEXT=text1234;"
      "S.android.intent.extra.SUBJECT=title1234;end";
  EXPECT_EQ(intent_str,
            apps_util::CreateLaunchIntent("com.android.vending", intent));

  apps::mojom::IntentPtr intent_with_text_and_title =
      apps_util::CreateShareIntentFromFiles(absl::nullopt, file_paths,
                                            mime_types);
  const std::string intent_with_text_and_title_str =
      "#Intent;action=android.intent.action.SEND;launchFlags=0x10200000;"
      "component=com.android.vending/;type=*/*;end";
  EXPECT_EQ(intent_with_text_and_title_str,
            apps_util::CreateLaunchIntent("com.android.vending",
                                          intent_with_text_and_title));
}
