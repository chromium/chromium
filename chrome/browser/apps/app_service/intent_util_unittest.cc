// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/intent_util.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/intent_common.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/intent_helper/intent_filter.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/strings/strcat.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/origin.h"
#include "url/url_constants.h"

class TestingProfile;
#endif

using apps::Condition;
using apps::ConditionType;
using apps::IntentFilterPtr;
using apps::IntentFilters;
using apps::PatternMatchType;

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

TEST_F(IntentUtilsTest, CreateIntentFiltersForWebApp_WebApp_HasUrlFilter) {
  auto web_app = web_app::test::CreateWebApp();
  DCHECK(web_app->start_url().is_valid());
  GURL scope = web_app->start_url().GetWithoutFilename();
  web_app->SetScope(scope);

  IntentFilters filters = apps_util::CreateIntentFiltersForWebApp(
      web_app->app_id(), /*is_note_taking_web_app*/ false, scope,
      /*app_share_target*/ nullptr, /*enabled_file_handlers*/ nullptr);

  ASSERT_EQ(filters.size(), 1u);
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
}

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
TEST_F(IntentUtilsTest, CreateWebAppIntentFilters_WebApp_HasUrlFilter) {
  auto web_app = web_app::test::CreateWebApp();
  DCHECK(web_app->start_url().is_valid());
  GURL scope = web_app->start_url().GetWithoutFilename();
  web_app->SetScope(scope);

  std::vector<apps::mojom::IntentFilterPtr> filters =
      apps_util::CreateWebAppIntentFilters(
          web_app->app_id(), /*is_note_taking_web_app*/ false, scope,
          /*app_share_target*/ nullptr, /*enabled_file_handlers*/ nullptr);

  ASSERT_EQ(filters.size(), 1u);
  apps::mojom::IntentFilterPtr& filter = filters[0];
  EXPECT_FALSE(filter->activity_name.has_value());
  EXPECT_FALSE(filter->activity_label.has_value());
  ASSERT_EQ(filter->conditions.size(), 4U);

  {
    const apps::mojom::Condition& condition = *filter->conditions[0];
    EXPECT_EQ(condition.condition_type, apps::mojom::ConditionType::kAction);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value,
              apps_util::kIntentActionView);
  }

  {
    const apps::mojom::Condition& condition = *filter->conditions[1];
    EXPECT_EQ(condition.condition_type, apps::mojom::ConditionType::kScheme);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, scope.scheme());
  }

  {
    const apps::mojom::Condition& condition = *filter->conditions[2];
    EXPECT_EQ(condition.condition_type, apps::mojom::ConditionType::kHost);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kNone);
    EXPECT_EQ(condition.condition_values[0]->value, scope.host());
  }

  {
    const apps::mojom::Condition& condition = *filter->conditions[3];
    EXPECT_EQ(condition.condition_type, apps::mojom::ConditionType::kPattern);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kPrefix);
    EXPECT_EQ(condition.condition_values[0]->value, scope.path());
  }

  EXPECT_TRUE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(web_app->start_url()), filter));

  EXPECT_FALSE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(GURL("https://bar.com")), filter));
}

TEST_F(IntentUtilsTest, CreateIntentFiltersForWebApp_FileHandlers) {
  auto web_app = web_app::test::CreateWebApp();
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

  IntentFilters filters = apps_util::CreateIntentFiltersForWebApp(
      web_app->app_id(), /*is_note_taking_web_app*/ false, scope,
      /*app_share_target*/ nullptr, &file_handlers);

  ASSERT_EQ(filters.size(), 2u);
  // 1st filter is URL filter.

  // File filter - View action
  const IntentFilterPtr& file_filter = filters[1];
  ASSERT_EQ(file_filter->conditions.size(), 2u);
  const Condition& view_cond = *file_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1u);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // File filter - mime & file extension match
  const Condition& file_cond = *file_filter->conditions[1];
  EXPECT_EQ(file_cond.condition_type, ConditionType::kFile);
  ASSERT_EQ(file_cond.condition_values.size(), 2u);
  EXPECT_EQ(file_cond.condition_values[0]->match_type,
            PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond.condition_values[0]->value, "text/plain");
  EXPECT_EQ(file_cond.condition_values[1]->match_type,
            PatternMatchType::kFileExtension);
  EXPECT_EQ(file_cond.condition_values[1]->value, ".txt");
}

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
TEST_F(IntentUtilsTest, CreateWebAppIntentFilters_FileHandlers) {
  auto web_app = web_app::test::CreateWebApp();
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

  std::vector<apps::mojom::IntentFilterPtr> filters =
      apps_util::CreateWebAppIntentFilters(
          web_app->app_id(), /*is_note_taking_web_app*/ false, scope,
          /*app_share_target*/ nullptr, &file_handlers);

  ASSERT_EQ(filters.size(), 2u);
  // 1st filter is URL filter.

  // File filter - View action
  const apps::mojom::IntentFilterPtr& file_filter = filters[1];
  ASSERT_EQ(file_filter->conditions.size(), 2u);
  const apps::mojom::Condition& view_cond = *file_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, apps::mojom::ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1u);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // File filter - mime & file extension match
  const apps::mojom::Condition& file_cond = *file_filter->conditions[1];
  EXPECT_EQ(file_cond.condition_type, apps::mojom::ConditionType::kFile);
  ASSERT_EQ(file_cond.condition_values.size(), 2u);
  EXPECT_EQ(file_cond.condition_values[0]->match_type,
            apps::mojom::PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond.condition_values[0]->value, "text/plain");
  EXPECT_EQ(file_cond.condition_values[1]->match_type,
            apps::mojom::PatternMatchType::kFileExtension);
  EXPECT_EQ(file_cond.condition_values[1]->value, ".txt");
}

TEST_F(IntentUtilsTest, CreateIntentFiltersForWebApp_NoteTakingApp) {
  auto web_app = web_app::test::CreateWebApp();
  DCHECK(web_app->start_url().is_valid());
  GURL scope = web_app->start_url().GetWithoutFilename();
  web_app->SetScope(scope);
  GURL new_note_url = scope.Resolve("/new_note.html");
  web_app->SetNoteTakingNewNoteUrl(new_note_url);

  IntentFilters filters = apps_util::CreateIntentFiltersForWebApp(
      web_app->app_id(), /*is_note_taking_web_app*/ true, scope,
      /*app_share_target*/ nullptr, /*enabled_file_handlers*/ nullptr);

  ASSERT_EQ(filters.size(), 2u);

  // 2nd filter is note-taking filter.
  ASSERT_EQ(filters[1]->conditions.size(), 1u);
  const Condition& condition = *filters[1]->conditions[0];
  EXPECT_EQ(condition.condition_type, ConditionType::kAction);
  ASSERT_EQ(condition.condition_values.size(), 1u);
  EXPECT_EQ(condition.condition_values[0]->value,
            apps_util::kIntentActionCreateNote);
}

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
TEST_F(IntentUtilsTest, CreateWebAppIntentFilters_NoteTakingApp) {
  auto web_app = web_app::test::CreateWebApp();
  DCHECK(web_app->start_url().is_valid());
  GURL scope = web_app->start_url().GetWithoutFilename();
  web_app->SetScope(scope);
  GURL new_note_url = scope.Resolve("/new_note.html");
  web_app->SetNoteTakingNewNoteUrl(new_note_url);

  std::vector<apps::mojom::IntentFilterPtr> filters =
      apps_util::CreateWebAppIntentFilters(
          web_app->app_id(), /*is_note_taking_web_app*/ true, scope,
          /*app_share_target*/ nullptr, /*enabled_file_handlers*/ nullptr);

  ASSERT_EQ(filters.size(), 2u);

  // 1st filter is URL filter.
  EXPECT_TRUE(apps_util::IntentMatchesFilter(
      apps_util::CreateIntentFromUrl(scope), filters[0]));

  // 2nd filter is note-taking filter.
  ASSERT_EQ(filters[1]->conditions.size(), 1u);
  const apps::mojom::Condition& condition = *filters[1]->conditions[0];
  EXPECT_EQ(condition.condition_type, apps::mojom::ConditionType::kAction);
  ASSERT_EQ(condition.condition_values.size(), 1u);
  EXPECT_EQ(condition.condition_values[0]->value,
            apps_util::kIntentActionCreateNote);
  EXPECT_TRUE(apps_util::IntentMatchesFilter(
      apps_util::CreateCreateNoteIntent(), filters[1]));
}

TEST_F(IntentUtilsTest, CreateIntentFiltersForChromeApp_FileHandlers) {
  // Foo app provides file handler for text/plain and all file types.
  extensions::ExtensionBuilder foo_app;
  foo_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", extensions::DictionaryBuilder()
                          .Set("background",
                               extensions::DictionaryBuilder()
                                   .Set("scripts", extensions::ListBuilder()
                                                       .Append("background.js")
                                                       .Build())
                                   .Build())
                          .Build())
          .Set(
              "file_handlers",
              extensions::DictionaryBuilder()
                  .Set("any",
                       extensions::DictionaryBuilder()
                           .Set("types",
                                extensions::ListBuilder().Append("*/*").Build())
                           .Build())
                  .Set("text",
                       extensions::DictionaryBuilder()
                           .Set("types", extensions::ListBuilder()
                                             .Append("text/plain")
                                             .Build())
                           .Set("extensions",
                                extensions::ListBuilder().Append("txt").Build())
                           .Set("verb", "open_with")
                           .Build())
                  .Build())
          .Build());
  foo_app.SetID("abcdzxcv");
  scoped_refptr<const extensions::Extension> foo = foo_app.Build();

  IntentFilters filters = apps_util::CreateIntentFiltersForChromeApp(foo.get());

  ASSERT_EQ(filters.size(), 2u);

  // "any" filter - View action
  const IntentFilterPtr& mime_filter = filters[0];
  ASSERT_EQ(mime_filter->conditions.size(), 2u);
  const Condition& view_cond = *mime_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1u);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // "any" filter - mime type match
  const Condition& file_cond = *mime_filter->conditions[1];
  EXPECT_EQ(file_cond.condition_type, ConditionType::kFile);
  ASSERT_EQ(file_cond.condition_values.size(), 1u);
  EXPECT_EQ(file_cond.condition_values[0]->match_type,
            PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond.condition_values[0]->value, "*/*");

  // Text filter - View action
  const IntentFilterPtr& mime_filter2 = filters[1];
  ASSERT_EQ(mime_filter2->conditions.size(), 2u);
  const Condition& view_cond2 = *mime_filter2->conditions[0];
  EXPECT_EQ(view_cond2.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond2.condition_values.size(), 1u);
  EXPECT_EQ(view_cond2.condition_values[0]->value,
            apps_util::kIntentActionView);

  // Text filter - mime type match
  const Condition& file_cond2 = *mime_filter2->conditions[1];
  EXPECT_EQ(file_cond2.condition_type, ConditionType::kFile);
  ASSERT_EQ(file_cond2.condition_values.size(), 2u);
  EXPECT_EQ(file_cond2.condition_values[0]->match_type,
            PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond2.condition_values[0]->value, "text/plain");
  // Text filter - file extension match
  EXPECT_EQ(file_cond2.condition_values[1]->match_type,
            PatternMatchType::kFileExtension);
  EXPECT_EQ(file_cond2.condition_values[1]->value, "txt");
}

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
TEST_F(IntentUtilsTest, CreateChromeAppIntentFilters_FileHandlers) {
  // Foo app provides file handler for text/plain and all file types.
  extensions::ExtensionBuilder foo_app;
  foo_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", extensions::DictionaryBuilder()
                          .Set("background",
                               extensions::DictionaryBuilder()
                                   .Set("scripts", extensions::ListBuilder()
                                                       .Append("background.js")
                                                       .Build())
                                   .Build())
                          .Build())
          .Set(
              "file_handlers",
              extensions::DictionaryBuilder()
                  .Set("any",
                       extensions::DictionaryBuilder()
                           .Set("types",
                                extensions::ListBuilder().Append("*/*").Build())
                           .Build())
                  .Set("text",
                       extensions::DictionaryBuilder()
                           .Set("types", extensions::ListBuilder()
                                             .Append("text/plain")
                                             .Build())
                           .Set("extensions",
                                extensions::ListBuilder().Append("txt").Build())
                           .Set("verb", "open_with")
                           .Build())
                  .Build())
          .Build());
  foo_app.SetID("abcdzxcv");
  scoped_refptr<const extensions::Extension> foo = foo_app.Build();

  std::vector<apps::mojom::IntentFilterPtr> filters =
      apps_util::CreateChromeAppIntentFilters(foo.get());

  ASSERT_EQ(filters.size(), 2u);

  // "any" filter - View action
  const apps::mojom::IntentFilterPtr& mime_filter = filters[0];
  ASSERT_EQ(mime_filter->conditions.size(), 2u);
  const apps::mojom::Condition& view_cond = *mime_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, apps::mojom::ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1u);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // "any" filter - mime type match
  const apps::mojom::Condition& file_cond = *mime_filter->conditions[1];
  EXPECT_EQ(file_cond.condition_type, apps::mojom::ConditionType::kFile);
  ASSERT_EQ(file_cond.condition_values.size(), 1u);
  EXPECT_EQ(file_cond.condition_values[0]->match_type,
            apps::mojom::PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond.condition_values[0]->value, "*/*");

  // Text filter - View action
  const apps::mojom::IntentFilterPtr& mime_filter2 = filters[1];
  ASSERT_EQ(mime_filter2->conditions.size(), 2u);
  const apps::mojom::Condition& view_cond2 = *mime_filter2->conditions[0];
  EXPECT_EQ(view_cond2.condition_type, apps::mojom::ConditionType::kAction);
  ASSERT_EQ(view_cond2.condition_values.size(), 1u);
  EXPECT_EQ(view_cond2.condition_values[0]->value,
            apps_util::kIntentActionView);

  // Text filter - mime type match
  const apps::mojom::Condition& file_cond2 = *mime_filter2->conditions[1];
  EXPECT_EQ(file_cond2.condition_type, apps::mojom::ConditionType::kFile);
  ASSERT_EQ(file_cond2.condition_values.size(), 2u);
  EXPECT_EQ(file_cond2.condition_values[0]->match_type,
            apps::mojom::PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond2.condition_values[0]->value, "text/plain");
  // Text filter - file extension match
  EXPECT_EQ(file_cond2.condition_values[1]->match_type,
            apps::mojom::PatternMatchType::kFileExtension);
  EXPECT_EQ(file_cond2.condition_values[1]->value, "txt");
}

TEST_F(IntentUtilsTest, CreateIntentFiltersForExtension_FileHandlers) {
  // Foo extension provides file_browser_handlers for html and anything.
  extensions::ExtensionBuilder foo_ext;
  foo_ext.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set(
              "background",
              extensions::DictionaryBuilder()
                  .Set(
                      "scripts",
                      extensions::ListBuilder().Append("background.js").Build())
                  .Set("persistent", false)
                  .Build())
          .Set(
              "file_browser_handlers",
              extensions::ListBuilder()
                  .Append(
                      extensions::DictionaryBuilder()
                          .Set("id", "open")
                          .Set("default_title", "Open me!")
                          .Set("file_filters", extensions::ListBuilder()
                                                   .Append("filesystem:*.html")
                                                   .Build())
                          .Build())
                  .Append(extensions::DictionaryBuilder()
                              .Set("id", "open_all")
                              .Set("default_title", "Open anything!")
                              .Set("file_filters", extensions::ListBuilder()
                                                       .Append("filesystem:*.*")
                                                       .Build())
                              .Build())
                  .Build())
          .Set("permissions",
               extensions::ListBuilder().Append("fileBrowserHandler").Build())
          .Build());

  foo_ext.SetID("abcdzxcv");
  scoped_refptr<const extensions::Extension> foo = foo_ext.Build();

  IntentFilters filters = apps_util::CreateIntentFiltersForExtension(foo.get());

  ASSERT_EQ(filters.size(), 2u);

  // "html" filter - View action
  const IntentFilterPtr& mime_filter = filters[0];
  ASSERT_EQ(mime_filter->conditions.size(), 2u);
  const Condition& view_cond = *mime_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1u);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // "html" filter - glob match
  const Condition& file_cond = *mime_filter->conditions[1];
  EXPECT_EQ(file_cond.condition_type, ConditionType::kFile);
  ASSERT_EQ(file_cond.condition_values.size(), 2u);
  EXPECT_EQ(file_cond.condition_values[0]->match_type, PatternMatchType::kGlob);
  EXPECT_EQ(file_cond.condition_values[0]->value,
            R"(filesystem:chrome-extension://.*/.*\.html)");
  EXPECT_EQ(file_cond.condition_values[1]->match_type, PatternMatchType::kGlob);
  EXPECT_EQ(file_cond.condition_values[1]->value,
            R"(filesystem:chrome://file-manager/.*\.html)");

  // "any" filter - View action
  const IntentFilterPtr& mime_filter2 = filters[1];
  ASSERT_EQ(mime_filter2->conditions.size(), 2u);
  const Condition& view_cond2 = *mime_filter2->conditions[0];
  EXPECT_EQ(view_cond2.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond2.condition_values.size(), 1u);
  EXPECT_EQ(view_cond2.condition_values[0]->value,
            apps_util::kIntentActionView);

  // "any" filter - glob match
  const Condition& file_cond2 = *mime_filter2->conditions[1];
  EXPECT_EQ(file_cond2.condition_type, ConditionType::kFile);
  ASSERT_EQ(file_cond2.condition_values.size(), 2u);
  EXPECT_EQ(file_cond2.condition_values[0]->match_type,
            PatternMatchType::kGlob);
  EXPECT_EQ(file_cond2.condition_values[0]->value,
            R"(filesystem:chrome-extension://.*/.*\..*)");
  EXPECT_EQ(file_cond2.condition_values[1]->match_type,
            PatternMatchType::kGlob);
  EXPECT_EQ(file_cond2.condition_values[1]->value,
            R"(filesystem:chrome://file-manager/.*\..*)");
}

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
TEST_F(IntentUtilsTest, CreateExtensionIntentFilters_FileHandlers) {
  // Foo extension provides file_browser_handlers for html and anything.
  extensions::ExtensionBuilder foo_ext;
  foo_ext.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set(
              "background",
              extensions::DictionaryBuilder()
                  .Set(
                      "scripts",
                      extensions::ListBuilder().Append("background.js").Build())
                  .Set("persistent", false)
                  .Build())
          .Set(
              "file_browser_handlers",
              extensions::ListBuilder()
                  .Append(
                      extensions::DictionaryBuilder()
                          .Set("id", "open")
                          .Set("default_title", "Open me!")
                          .Set("file_filters", extensions::ListBuilder()
                                                   .Append("filesystem:*.html")
                                                   .Build())
                          .Build())
                  .Append(extensions::DictionaryBuilder()
                              .Set("id", "open_all")
                              .Set("default_title", "Open anything!")
                              .Set("file_filters", extensions::ListBuilder()
                                                       .Append("filesystem:*.*")
                                                       .Build())
                              .Build())
                  .Build())
          .Set("permissions",
               extensions::ListBuilder().Append("fileBrowserHandler").Build())
          .Build());

  foo_ext.SetID("abcdzxcv");
  scoped_refptr<const extensions::Extension> foo = foo_ext.Build();

  std::vector<apps::mojom::IntentFilterPtr> filters =
      apps_util::CreateExtensionIntentFilters(foo.get());

  ASSERT_EQ(filters.size(), 2u);

  // "html" filter - View action
  const apps::mojom::IntentFilterPtr& mime_filter = filters[0];
  ASSERT_EQ(mime_filter->conditions.size(), 2u);
  const apps::mojom::Condition& view_cond = *mime_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, apps::mojom::ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1u);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // "html" filter - glob match
  const apps::mojom::Condition& file_cond = *mime_filter->conditions[1];
  EXPECT_EQ(file_cond.condition_type, apps::mojom::ConditionType::kFile);
  ASSERT_EQ(file_cond.condition_values.size(), 2u);
  EXPECT_EQ(file_cond.condition_values[0]->match_type,
            apps::mojom::PatternMatchType::kGlob);
  EXPECT_EQ(file_cond.condition_values[0]->value,
            R"(filesystem:chrome-extension://.*/.*\.html)");
  EXPECT_EQ(file_cond.condition_values[1]->match_type,
            apps::mojom::PatternMatchType::kGlob);
  EXPECT_EQ(file_cond.condition_values[1]->value,
            R"(filesystem:chrome://file-manager/.*\.html)");

  // "any" filter - View action
  const apps::mojom::IntentFilterPtr& mime_filter2 = filters[1];
  ASSERT_EQ(mime_filter2->conditions.size(), 2u);
  const apps::mojom::Condition& view_cond2 = *mime_filter2->conditions[0];
  EXPECT_EQ(view_cond2.condition_type, apps::mojom::ConditionType::kAction);
  ASSERT_EQ(view_cond2.condition_values.size(), 1u);
  EXPECT_EQ(view_cond2.condition_values[0]->value,
            apps_util::kIntentActionView);

  // "any" filter - glob match
  const apps::mojom::Condition& file_cond2 = *mime_filter2->conditions[1];
  EXPECT_EQ(file_cond2.condition_type, apps::mojom::ConditionType::kFile);
  ASSERT_EQ(file_cond2.condition_values.size(), 2u);
  EXPECT_EQ(file_cond2.condition_values[0]->match_type,
            apps::mojom::PatternMatchType::kGlob);
  EXPECT_EQ(file_cond2.condition_values[0]->value,
            R"(filesystem:chrome-extension://.*/.*\..*)");
  EXPECT_EQ(file_cond2.condition_values[1]->match_type,
            apps::mojom::PatternMatchType::kGlob);
  EXPECT_EQ(file_cond2.condition_values[1]->value,
            R"(filesystem:chrome://file-manager/.*\..*)");
}

// Converting an Arc Intent filter for a URL view intent filter should add a
// condition covering every possible path.
TEST_F(IntentUtilsTest, ConvertArcIntentFilter_AddsMissingPath) {
  const char* kPackageName = "com.foo.bar";
  const char* kHost = "www.google.com";
  const char* kPath = "/";
  const char* kScheme = "https";

  std::vector<arc::IntentFilter::AuthorityEntry> authorities1;
  authorities1.emplace_back(kHost, 0);
  std::vector<arc::IntentFilter::PatternMatcher> patterns;
  patterns.emplace_back(kPath, arc::mojom::PatternType::PATTERN_PREFIX);

  arc::IntentFilter filter_with_path(kPackageName, {arc::kIntentActionView},
                                     std::move(authorities1),
                                     std::move(patterns), {kScheme}, {});

  apps::mojom::IntentFilterPtr app_service_filter1 =
      apps_util::ConvertArcToAppServiceIntentFilter(filter_with_path);

  std::vector<arc::IntentFilter::AuthorityEntry> authorities2;
  authorities2.emplace_back(kHost, 0);
  arc::IntentFilter filter_without_path(kPackageName, {arc::kIntentActionView},
                                        std::move(authorities2), {}, {kScheme},
                                        {});

  apps::mojom::IntentFilterPtr app_service_filter2 =
      apps_util::ConvertArcToAppServiceIntentFilter(filter_without_path);

  ASSERT_EQ(app_service_filter1, app_service_filter2);
}

TEST_F(IntentUtilsTest, ConvertArcIntentFilter_ConvertsSimpleGlobToPrefix) {
  const char* kPackageName = "com.foo.bar";
  const char* kHost = "www.google.com";
  const char* kScheme = "https";

  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back(kHost, 0);

  std::vector<arc::IntentFilter::PatternMatcher> patterns;

  patterns.emplace_back("/foo.*", arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  patterns.emplace_back(".*", arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  patterns.emplace_back("/foo/.*/bar",
                        arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  patterns.emplace_back("/..*", arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);

  arc::IntentFilter filter_with_path(kPackageName, {arc::kIntentActionView},
                                     std::move(authorities),
                                     std::move(patterns), {kScheme}, {});

  apps::mojom::IntentFilterPtr app_service_filter =
      apps_util::ConvertArcToAppServiceIntentFilter(filter_with_path);

  for (auto& condition : app_service_filter->conditions) {
    if (condition->condition_type == apps::mojom::ConditionType::kPattern) {
      EXPECT_EQ(4u, condition->condition_values.size());
      EXPECT_EQ(apps_util::MakeConditionValue(
                    "/foo", apps::mojom::PatternMatchType::kPrefix),
                condition->condition_values[0]);
      EXPECT_EQ(apps_util::MakeConditionValue(
                    std::string(), apps::mojom::PatternMatchType::kPrefix),
                condition->condition_values[1]);
      EXPECT_EQ(apps_util::MakeConditionValue(
                    "/foo/.*/bar", apps::mojom::PatternMatchType::kGlob),
                condition->condition_values[2]);
      EXPECT_EQ(apps_util::MakeConditionValue(
                    "/..*", apps::mojom::PatternMatchType::kGlob),
                condition->condition_values[3]);
    }
  }
}

TEST_F(IntentUtilsTest, ConvertArcIntentFilter_DeduplicatesHosts) {
  const char* kPackageName = "com.foo.bar";
  const char* kPath = "/";
  const char* kScheme = "https";
  const char* kHost1 = "www.a.com";
  const char* kHost2 = "www.b.com";

  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back(kHost1, 0);
  authorities.emplace_back(kHost2, 0);
  authorities.emplace_back(kHost2, 0);
  authorities.emplace_back(kHost1, 0);

  std::vector<arc::IntentFilter::PatternMatcher> patterns;
  patterns.emplace_back(kPath, arc::mojom::PatternType::PATTERN_PREFIX);

  arc::IntentFilter arc_filter(kPackageName, {arc::kIntentActionView},
                               std::move(authorities), std::move(patterns),
                               {kScheme}, {});

  apps::mojom::IntentFilterPtr app_service_filter =
      apps_util::ConvertArcToAppServiceIntentFilter(arc_filter);

  for (auto& condition : app_service_filter->conditions) {
    if (condition->condition_type == apps::mojom::ConditionType::kHost) {
      ASSERT_EQ(2u, condition->condition_values.size());
      ASSERT_EQ(kHost1, condition->condition_values[0]->value);
      ASSERT_EQ(kHost2, condition->condition_values[1]->value);
    }
  }
}

TEST_F(IntentUtilsTest, ConvertArcIntentFilter_WildcardHostPatternMatchType) {
  const char* kPackageName = "com.foo.bar";
  const char* kPath = "/";
  const char* kScheme = "https";
  const char* kHostWildcard = "*.google.com";
  const char* kHostNoWildcard = "google.com";

  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back(kHostWildcard, 0);
  authorities.emplace_back(kHostNoWildcard, 0);
  std::vector<arc::IntentFilter::PatternMatcher> patterns;
  patterns.emplace_back(kPath, arc::mojom::PatternType::PATTERN_PREFIX);

  arc::IntentFilter arc_filter(kPackageName, {arc::kIntentActionView},
                               std::move(authorities), std::move(patterns),
                               {kScheme}, {});

  apps::mojom::IntentFilterPtr app_service_filter =
      apps_util::ConvertArcToAppServiceIntentFilter(arc_filter);

  for (auto& condition : app_service_filter->conditions) {
    if (condition->condition_type == apps::mojom::ConditionType::kHost) {
      ASSERT_EQ(condition->condition_values.size(), 2U);

      // Check wildcard host
      EXPECT_EQ(condition->condition_values[0]->match_type,
                ConvertPatternMatchTypeToMojomPatternMatchType(
                    apps::PatternMatchType::kSuffix));
      // Check non-wildcard host
      EXPECT_EQ(condition->condition_values[1]->match_type,
                ConvertPatternMatchTypeToMojomPatternMatchType(
                    apps::PatternMatchType::kNone));
    }
  }
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(IntentUtilsTest, CrosapiIntentConversion) {
  apps::mojom::IntentPtr original_intent =
      apps_util::CreateIntentFromUrl(GURL("www.google.com"));
  auto crosapi_intent =
      apps_util::ConvertAppServiceToCrosapiIntent(original_intent, nullptr);
  auto converted_intent =
      apps_util::ConvertCrosapiToAppServiceIntent(crosapi_intent, nullptr);
  EXPECT_EQ(original_intent, converted_intent);

  original_intent = apps_util::CreateShareIntentFromText("text", "title");
  crosapi_intent =
      apps_util::ConvertAppServiceToCrosapiIntent(original_intent, nullptr);
  converted_intent =
      apps_util::ConvertCrosapiToAppServiceIntent(crosapi_intent, nullptr);
  EXPECT_EQ(original_intent, converted_intent);
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
class IntentUtilsFileTest : public ::testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing_profile");
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            mount_name_, storage::FileSystemType::kFileSystemTypeExternal,
            storage::FileSystemMountOption(), base::FilePath(fs_root_)));
  }

  void TearDown() override {
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
            mount_name_));
    profile_manager_->DeleteAllTestingProfiles();
    profile_ = nullptr;
    profile_manager_.reset();
  }

  TestingProfile* GetProfile() { return profile_; }

  // FileUtils explicitly relies on ChromeOS Files.app for files manipulation.
  const url::Origin GetFileManagerOrigin() {
    return url::Origin::Create(file_manager::util::GetFileManagerURL());
  }

  // For a given |root| converts the given virtual |path| to a GURL.
  GURL ToGURL(const base::FilePath& root, const std::string& path) {
    const std::string abs_path = root.Append(path).value();
    return GURL(base::StrCat({url::kFileSystemScheme, ":",
                              GetFileManagerOrigin().Serialize(), abs_path}));
  }

 protected:
  const std::string mount_name_ = "TestMountName";
  const std::string fs_root_ = "/path/to/test/filesystemroot";

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
};

TEST_F(IntentUtilsFileTest, AppServiceIntentToCrosapi) {
  auto app_service_intent = apps::mojom::Intent::New();
  app_service_intent->action = "action";
  app_service_intent->mime_type = "*/*";
  const std::string path = "Documents/foo.txt";
  const std::string mime_type = "text/plain";
  auto url = ToGURL(base::FilePath(storage::kTestDir), path);
  app_service_intent->files = std::vector<apps::mojom::IntentFilePtr>{};
  auto file = apps::mojom::IntentFile::New();
  file->url = url;
  file->mime_type = mime_type;
  app_service_intent->files->push_back(std::move(file));
  auto crosapi_intent = apps_util::ConvertAppServiceToCrosapiIntent(
      app_service_intent, GetProfile());
  EXPECT_EQ(app_service_intent->action, crosapi_intent->action);
  EXPECT_EQ(app_service_intent->mime_type, crosapi_intent->mime_type);
  ASSERT_TRUE(crosapi_intent->files.has_value());
  ASSERT_EQ(crosapi_intent->files.value().size(), 1U);
  EXPECT_EQ(crosapi_intent->files.value()[0]->file_path, base::FilePath(path));
  EXPECT_EQ(crosapi_intent->files.value()[0]->mime_type, mime_type);
}

TEST_F(IntentUtilsFileTest, CrosapiIntentToAppService) {
  const std::string path = "Documents/foo.txt";
  auto file_path = base::FilePath(fs_root_).Append(path);
  auto file_paths = apps::mojom::FilePaths::New();
  file_paths->file_paths.push_back(file_path);
  auto crosapi_intent = apps_util::CreateCrosapiIntentForViewFiles(file_paths);

  auto app_service_intent =
      apps_util::ConvertCrosapiToAppServiceIntent(crosapi_intent, GetProfile());
  EXPECT_EQ(app_service_intent->action, crosapi_intent->action);
  EXPECT_EQ(app_service_intent->mime_type, crosapi_intent->mime_type);
  ASSERT_TRUE(crosapi_intent->files.has_value());
  ASSERT_EQ(crosapi_intent->files.value().size(), 1U);
  EXPECT_EQ(
      app_service_intent->files.value()[0]->url,
      ToGURL(base::FilePath(storage::kExternalDir).Append(mount_name_), path));
}
#endif
