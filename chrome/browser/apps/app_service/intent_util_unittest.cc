// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/intent_util.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/intent_helper/intent_filter.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/intent_common.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "net/base/filename_util.h"
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(IntentUtilsTest, CreateIntentForActivity) {
  const std::string& activity_name = "com.android.vending.AssetBrowserActivity";
  const std::string& start_type = "initialStart";
  const std::string& category = "android.intent.category.LAUNCHER";
  apps::IntentPtr intent =
      apps_util::MakeIntentForActivity(activity_name, start_type, category);
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

TEST_F(IntentUtilsTest, CreateArcIntentExtras) {
  // Test the case where both `share_type` and `extras` are filled in in intent.
  const std::string& activity_name = "com.android.vending.AssetBrowserActivity";
  const std::string& start_type = "initialStart";
  const std::string& category = "android.intent.category.LAUNCHER";
  apps::IntentPtr intent =
      apps_util::MakeIntentForActivity(activity_name, start_type, category);
  // Add extras other than share text, share type nor share title.
  const std::string& extra_name = "android.intent.extra.TESTING";
  const std::string& extra_value = "testing";
  intent->extras = base::flat_map<std::string, std::string>();
  intent->extras.insert(std::make_pair(extra_name, extra_value));

  arc::mojom::IntentInfoPtr arc_intent =
      apps_util::ConvertAppServiceToArcIntent(intent);

  ASSERT_TRUE(intent);
  ASSERT_TRUE(arc_intent);

  std::string intent_str =
      "#Intent;action=android.intent.action.MAIN;category=android.intent."
      "category.LAUNCHER;launchFlags=0x10200000;component=com.android.vending/"
      ".AssetBrowserActivity;S.org.chromium.arc.start_type=initialStart;"
      "android.intent.extra.TESTING=testing;end";
  EXPECT_EQ(intent_str,
            apps_util::CreateLaunchIntent("com.android.vending", intent));

  EXPECT_EQ(arc::kIntentActionMain, arc_intent->action);

  // Check both share_type and extras exist in `arc_intent->extras`.
  base::flat_map<std::string, std::string> extras;
  extras.insert(std::make_pair("org.chromium.arc.start_type", start_type));
  extras.insert(std::make_pair(extra_name, extra_value));
  EXPECT_TRUE(arc_intent->extras.has_value());
  EXPECT_EQ(extras, arc_intent->extras);

  EXPECT_TRUE(arc_intent->categories.has_value());
  EXPECT_EQ(category, arc_intent->categories.value()[0]);

  arc_intent->extras = apps_util::CreateArcIntentExtras(intent);
  EXPECT_TRUE(intent->activity_name.has_value());
  EXPECT_EQ(activity_name, intent->activity_name.value());
}

TEST_F(IntentUtilsTest, CreateShareIntentFromText) {
  apps::IntentPtr intent = apps_util::MakeShareIntent("text", "title");
  std::string intent_str =
      "#Intent;action=android.intent.action.SEND;launchFlags=0x10200000;"
      "component=com.android.vending/;type=text/"
      "plain;S.android.intent.extra.TEXT=text;S.android.intent.extra.SUBJECT="
      "title;end";
  EXPECT_EQ(intent_str,
            apps_util::CreateLaunchIntent("com.android.vending", intent));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(IntentUtilsTest, CreateNoteTakingFilter) {
  IntentFilterPtr filter = apps_util::CreateNoteTakingFilter();

  ASSERT_EQ(filter->conditions.size(), 1u);
  const Condition& condition = *filter->conditions[0];
  EXPECT_EQ(condition.condition_type, ConditionType::kAction);
  ASSERT_EQ(condition.condition_values.size(), 1u);
  EXPECT_EQ(condition.condition_values[0]->value,
            apps_util::kIntentActionCreateNote);

  EXPECT_TRUE(apps_util::CreateCreateNoteIntent()->MatchFilter(filter));
}

TEST_F(IntentUtilsTest, CreateLockScreenFilter) {
  IntentFilterPtr filter = apps_util::CreateLockScreenFilter();

  ASSERT_EQ(filter->conditions.size(), 1u);
  const Condition& condition = *filter->conditions[0];
  EXPECT_EQ(condition.condition_type, ConditionType::kAction);
  ASSERT_EQ(condition.condition_values.size(), 1u);
  EXPECT_EQ(condition.condition_values[0]->value,
            apps_util::kIntentActionStartOnLockScreen);

  EXPECT_TRUE(apps_util::CreateStartOnLockScreenIntent()->MatchFilter(filter));
}

TEST_F(IntentUtilsTest, CreateIntentFiltersForChromeApp_FileHandlers) {
  // Foo app provides file handler for text/plain and all file types.
  extensions::ExtensionBuilder foo_app("Foo");
  static constexpr char kManifest[] = R"(
    "manifest_version": 2,
    "version": "1.0.0",
    "app": {
      "background": {
        "scripts": ["background.js"]
      }
    },
    "file_handlers": {
      "any": {
        "types": ["*/*"]
      },
      "text": {
        "extensions": ["txt"],
        "types": ["text/plain"],
        "verb": "open_with"
      }
    }
  )";
  foo_app.AddJSON(kManifest).BuildManifest();
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(IntentUtilsTest, CreateIntentFiltersForChromeApp_NoteTaking) {
  const std::string note_action_handler =
      extensions::api::app_runtime::ToString(
          extensions::api::app_runtime::ActionType::kNewNote);
  // Foo app has a note-taking action handler.
  extensions::ExtensionBuilder foo_app("Foo");
  std::string manifest = base::StringPrintf(R"(
    "manifest_version": 2,
    "version": "1.0.0",
    "action_handlers": ["%s"],
    "app": {"background": {"scripts": ["background.js"]}}
  )",
                                            note_action_handler.c_str());
  foo_app.AddJSON(manifest).BuildManifest();
  scoped_refptr<const extensions::Extension> foo = foo_app.Build();

  IntentFilters filters = apps_util::CreateIntentFiltersForChromeApp(foo.get());

  ASSERT_EQ(filters.size(), 1u);
  const IntentFilterPtr& filter = filters[0];
  ASSERT_EQ(filter->conditions.size(), 1u);
  const Condition& condition = *filter->conditions[0];
  EXPECT_EQ(condition.condition_type, ConditionType::kAction);
  ASSERT_EQ(condition.condition_values.size(), 1u);
  EXPECT_EQ(condition.condition_values[0]->value,
            apps_util::kIntentActionCreateNote);

  apps::IntentPtr intent = apps_util::CreateCreateNoteIntent();
  EXPECT_TRUE(intent->MatchFilter(filter));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(IntentUtilsTest, CreateIntentFiltersForExtension_FileHandlers) {
  // Foo extension provides file_browser_handlers for html and anything.
  extensions::ExtensionBuilder foo_ext("Foo");
  static constexpr char kManifest[] = R"(
    "manifest_version": 2,
    "permissions": ["fileBrowserHandler"],
    "version": "1.0.0",
    "background": {
      "persistent": false,
      "scripts": ["background.js"]
    },
    "file_browser_handlers": [ {
      "default_title": "Open me!",
      "file_filters": ["filesystem:*.html"],
      "id": "open"
    }, {
      "default_title": "Open anything!",
      "file_filters": ["filesystem:*.*"],
      "id": "open_all"
    }]
  )";
  foo_ext.AddJSON(kManifest).BuildManifest();
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

class IntentUtilsForExtensionsTest : public IntentUtilsTest {
 public:
  IntentUtilsForExtensionsTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionWebFileHandlers);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(IntentUtilsForExtensionsTest,
       CreateIntentFiltersForExtension_WebFileHandlers) {
  // Create extension that provides file_handlers.
  extensions::ExtensionBuilder extension_builder("Test");
  static const char kManifest[] = R"(
      "version": "0.0.1",
      "manifest_version": 3,
      "file_handlers": [
        {
          "name": "Comma separated values",
          "action": "/open-csv.html",
          "accept": {"text/csv": ".csv"}
        },
        {
          "name": "Text file",
          "action": "/open-txt",
          "accept": {"text/plain": ".txt"}
        }
      ]
    )";
  extension_builder.AddJSON(kManifest).BuildManifest();
  scoped_refptr<const extensions::Extension> extension =
      extension_builder.Build();

  // Get intent filters.
  IntentFilters filters =
      apps_util::CreateIntentFiltersForExtension(extension.get());
  ASSERT_EQ(filters.size(), 2u);

  // "csv" filter - View action
  const IntentFilterPtr& mime_filter = filters[0];
  ASSERT_EQ(mime_filter->conditions.size(), 2u);
  const Condition& view_cond = *mime_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1u);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // "csv" filter - glob match
  const Condition& file_cond = *mime_filter->conditions[1];
  EXPECT_EQ(file_cond.condition_type, ConditionType::kFile);
  ASSERT_EQ(file_cond.condition_values.size(), 2u);
  EXPECT_EQ(file_cond.condition_values[0]->match_type,
            PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond.condition_values[0]->value, "text/csv");
  EXPECT_EQ(file_cond.condition_values[1]->match_type,
            PatternMatchType::kFileExtension);
  EXPECT_EQ(file_cond.condition_values[1]->value, ".csv");
}

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  apps::IntentFilterPtr app_service_filter1 =
      apps_util::CreateIntentFilterForArc(filter_with_path);

  std::vector<arc::IntentFilter::AuthorityEntry> authorities2;
  authorities2.emplace_back(kHost, 0);
  arc::IntentFilter filter_without_path(kPackageName, {arc::kIntentActionView},
                                        std::move(authorities2), {}, {kScheme},
                                        {});

  apps::IntentFilterPtr app_service_filter2 =
      apps_util::CreateIntentFilterForArc(filter_without_path);

  ASSERT_EQ(*app_service_filter1, *app_service_filter2);
}

// Converting an Arc Intent filter with invalid path.
TEST_F(IntentUtilsTest, ConvertArcIntentFilter_InvalidPath) {
  const char* kPackageName = "com.foo.bar";
  const char* kHost = "www.google.com";
  const char* kPath = "/";
  const char* kScheme = "https";

  // If all paths are invalid, return nullptr.
  std::vector<arc::IntentFilter::AuthorityEntry> authorities1;
  authorities1.emplace_back(kHost, 0);
  std::vector<arc::IntentFilter::PatternMatcher> patterns1;
  int invalid_pattern_type =
      static_cast<int>(arc::mojom::PatternType::kMaxValue) + 1;
  patterns1.emplace_back(
      kPath, static_cast<arc::mojom::PatternType>(invalid_pattern_type));

  arc::IntentFilter filter_with_only_invalid_path(
      kPackageName, {arc::kIntentActionView}, std::move(authorities1),
      std::move(patterns1), {kScheme}, {});

  apps::IntentFilterPtr app_service_filter1 =
      apps_util::CreateIntentFilterForArc(filter_with_only_invalid_path);

  ASSERT_FALSE(app_service_filter1);

  // If at least one path is valid, return intent filter with the valid path.
  std::vector<arc::IntentFilter::AuthorityEntry> authorities2;
  authorities2.emplace_back(kHost, 0);
  std::vector<arc::IntentFilter::PatternMatcher> patterns2;
  patterns2.emplace_back(
      kPath, static_cast<arc::mojom::PatternType>(invalid_pattern_type));
  patterns2.emplace_back(kPath, arc::mojom::PatternType::PATTERN_PREFIX);
  arc::IntentFilter filter_with_some_valid_path(
      kPackageName, {arc::kIntentActionView}, std::move(authorities2),
      std::move(patterns2), {kScheme}, {});

  apps::IntentFilterPtr app_service_filter2 =
      apps_util::CreateIntentFilterForArc(filter_with_some_valid_path);

  std::vector<arc::IntentFilter::AuthorityEntry> authorities3;
  authorities3.emplace_back(kHost, 0);
  std::vector<arc::IntentFilter::PatternMatcher> patterns3;
  patterns3.emplace_back(kPath, arc::mojom::PatternType::PATTERN_PREFIX);
  arc::IntentFilter filter_with_valid_path(
      kPackageName, {arc::kIntentActionView}, std::move(authorities3),
      std::move(patterns3), {kScheme}, {});

  apps::IntentFilterPtr app_service_filter3 =
      apps_util::CreateIntentFilterForArc(filter_with_valid_path);

  ASSERT_EQ(*app_service_filter2, *app_service_filter3);
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

  apps::IntentFilterPtr app_service_filter =
      apps_util::CreateIntentFilterForArc(filter_with_path);

  for (auto& condition : app_service_filter->conditions) {
    if (condition->condition_type == apps::ConditionType::kPath) {
      EXPECT_EQ(4u, condition->condition_values.size());
      EXPECT_EQ(apps::ConditionValue("/foo", apps::PatternMatchType::kPrefix),
                *condition->condition_values[0]);
      EXPECT_EQ(
          apps::ConditionValue(std::string(), apps::PatternMatchType::kPrefix),
          *condition->condition_values[1]);
      EXPECT_EQ(
          apps::ConditionValue("/foo/.*/bar", apps::PatternMatchType::kGlob),
          *condition->condition_values[2]);
      EXPECT_EQ(apps::ConditionValue("/..*", apps::PatternMatchType::kGlob),
                *condition->condition_values[3]);
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

  apps::IntentFilterPtr app_service_filter =
      apps_util::CreateIntentFilterForArc(arc_filter);

  for (auto& condition : app_service_filter->conditions) {
    if (condition->condition_type == apps::ConditionType::kAuthority) {
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

  apps::IntentFilterPtr app_service_filter =
      apps_util::CreateIntentFilterForArc(arc_filter);

  for (auto& condition : app_service_filter->conditions) {
    if (condition->condition_type == apps::ConditionType::kAuthority) {
      ASSERT_EQ(condition->condition_values.size(), 2U);

      // Check wildcard host
      EXPECT_EQ(condition->condition_values[0]->match_type,
                apps::PatternMatchType::kSuffix);
      // Check non-wildcard host
      EXPECT_EQ(condition->condition_values[1]->match_type,
                apps::PatternMatchType::kLiteral);
    }
  }
}

TEST_F(IntentUtilsTest, ConvertArcIntentFilter_FileIntentFilterScheme) {
  const char* kPackageName = "com.foo.bar";
  const char* kScheme = "content";
  const char* kMimeType = "image/*";

  arc::IntentFilter arc_filter(kPackageName, {arc::kIntentActionView}, {}, {},
                               {kScheme}, {kMimeType});

  apps::IntentFilterPtr app_service_filter =
      apps_util::CreateIntentFilterForArc(arc_filter);

  // There should be no scheme condition in the resulting App Service filter.
  ASSERT_EQ(app_service_filter->conditions.size(), 2U);
  for (auto& condition : app_service_filter->conditions) {
    ASSERT_TRUE(condition->condition_type != apps::ConditionType::kScheme);
    if (condition->condition_type == apps::ConditionType::kAction) {
      ASSERT_EQ(condition->condition_values[0]->value,
                apps_util::kIntentActionView);
    }
    if (condition->condition_type == apps::ConditionType::kMimeType) {
      ASSERT_EQ(condition->condition_values[0]->value, kMimeType);
    }
  }
}

TEST_F(IntentUtilsTest,
       ConvertArcIntentFilter_ConvertsPathToFileExtensionCondition) {
  const char* kPackageName = "com.foo.bar";
  const char* kPath = ".*\\.xyz";
  const char* kScheme = "content";
  const char* kMimeType = "*/*";
  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back("*", 0);

  std::vector<arc::IntentFilter::PatternMatcher> patterns;
  patterns.emplace_back(kPath, arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  arc::IntentFilter arc_filter(kPackageName, {arc::kIntentActionView},
                               std::move(authorities), std::move(patterns),
                               {kScheme}, {kMimeType});

  apps::IntentFilterPtr app_service_filter =
      apps_util::CreateIntentFilterForArc(arc_filter);
  ASSERT_TRUE(app_service_filter);

  // There should only be 2 conditions (host, scheme, mime type should be
  // omitted in the App Service filter).
  ASSERT_EQ(app_service_filter->conditions.size(), 2U);
  ASSERT_EQ(app_service_filter->conditions[0]->condition_type,
            apps::ConditionType::kAction);
  ASSERT_EQ(app_service_filter->conditions[1]->condition_type,
            apps::ConditionType::kFile);
  ASSERT_EQ(app_service_filter->conditions[1]->condition_values[0]->value,
            "xyz");
  ASSERT_EQ(app_service_filter->conditions[1]->condition_values[0]->match_type,
            apps::PatternMatchType::kFileExtension);
}

TEST_F(IntentUtilsTest,
       ConvertArcIntentFilter_FileExtensionFilterWithNoValidPath) {
  const char* kPackageName = "com.foo.bar";
  const char* kPath = "something/something.txt";
  const char* kScheme = "content";
  const char* kMimeType = "*";
  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back("*", 0);

  std::vector<arc::IntentFilter::PatternMatcher> patterns;
  patterns.emplace_back(kPath, arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  arc::IntentFilter arc_filter(kPackageName, {arc::kIntentActionView},
                               std::move(authorities), std::move(patterns),
                               {kScheme}, {kMimeType});

  apps::IntentFilterPtr app_service_filter =
      apps_util::CreateIntentFilterForArc(arc_filter);
  ASSERT_FALSE(app_service_filter);
}

TEST_F(IntentUtilsTest, ConvertValidFilePathsToFileExtensions) {
  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back("*", 0);
  std::vector<arc::IntentFilter::PatternMatcher> patterns;

  // Invalid paths.
  patterns.emplace_back("something/something.mp4",
                        arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  patterns.emplace_back(".*\\.\\\a.mp3",
                        arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  patterns.emplace_back(".*\\.none",
                        arc::mojom::PatternType::PATTERN_ADVANCED_GLOB);
  patterns.emplace_back(".*\\.xyz", arc::mojom::PatternType::PATTERN_LITERAL);
  patterns.emplace_back("hello.txt",
                        arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  patterns.emplace_back(".*\\.abc", arc::mojom::PatternType::PATTERN_SUFFIX);

  // Valid paths.
  patterns.emplace_back(".*\\..*\\.jpg",
                        arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  patterns.emplace_back(".*\\..*\\..*\\.png",
                        arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  patterns.emplace_back(".*\\..*\\..*\\..*\\..*\\.tar.gz",
                        arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);
  patterns.emplace_back(".*\\..*\\.my-file",
                        arc::mojom::PatternType::PATTERN_SIMPLE_GLOB);

  apps::IntentFilterPtr app_service_filter =
      apps_util::CreateIntentFilterForArc(arc::IntentFilter(
          "com.foo.bar", {arc::kIntentActionView}, std::move(authorities),
          std::move(patterns), {"content"}, {"*"}));

  ASSERT_TRUE(app_service_filter);
  ASSERT_EQ(app_service_filter->conditions.size(), 2U);
  ASSERT_EQ(app_service_filter->conditions[0]->condition_type,
            apps::ConditionType::kAction);
  ASSERT_EQ(app_service_filter->conditions[1]->condition_type,
            apps::ConditionType::kFile);

  apps::ConditionValues& result_extensions =
      app_service_filter->conditions[1]->condition_values;
  auto found_extension = [&result_extensions](const std::string extension) {
    return base::ranges::any_of(
        result_extensions,
        [extension](std::unique_ptr<apps::ConditionValue>& condition_value) {
          return condition_value->value == extension;
        });
  };

  EXPECT_EQ(result_extensions.size(), 4U);
  EXPECT_TRUE(found_extension("jpg"));
  EXPECT_TRUE(found_extension("png"));
  EXPECT_TRUE(found_extension("tar.gz"));
  EXPECT_TRUE(found_extension("my-file"));
}

TEST_F(IntentUtilsTest, ConvertArcIntentFilter_ReturnskFile) {
  const char* package_name = "com.foo.bar";
  const char* mime_type = "image/*";

  arc::IntentFilter arc_filter(package_name, {arc::kIntentActionView}, {}, {},
                               {}, {mime_type});

  apps::IntentFilterPtr app_service_filter =
      apps_util::CreateIntentFilterForArc(arc_filter);

  ASSERT_EQ(app_service_filter->conditions.size(), 2U);
  for (auto& condition : app_service_filter->conditions) {
    // There should not be a kMimeType condition for ARC view file intent
    // filters.
    ASSERT_NE(condition->condition_type, apps::ConditionType::kMimeType);
    if (condition->condition_type == apps::ConditionType::kAction) {
      ASSERT_EQ(condition->condition_values[0]->value,
                apps_util::kIntentActionView);
    }
    if (condition->condition_type == apps::ConditionType::kFile) {
      ASSERT_EQ(condition->condition_values[0]->value, mime_type);
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(IntentUtilsTest, CrosapiIntentConversion) {
  apps::IntentPtr original_intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView, GURL("www.google.com"));
  auto crosapi_intent =
      apps_util::ConvertAppServiceToCrosapiIntent(original_intent, nullptr);
  auto converted_intent =
      apps_util::CreateAppServiceIntentFromCrosapi(crosapi_intent, nullptr);
  EXPECT_EQ(*original_intent, *converted_intent);

  original_intent = apps_util::MakeShareIntent("text", "title");
  crosapi_intent =
      apps_util::ConvertAppServiceToCrosapiIntent(original_intent, nullptr);
  converted_intent =
      apps_util::CreateAppServiceIntentFromCrosapi(crosapi_intent, nullptr);
  EXPECT_EQ(*original_intent, *converted_intent);

  original_intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView);
  original_intent->data = "geo:0,0?q=1600%20amphitheatre%20parkway";
  crosapi_intent =
      apps_util::ConvertAppServiceToCrosapiIntent(original_intent, nullptr);
  converted_intent =
      apps_util::CreateAppServiceIntentFromCrosapi(crosapi_intent, nullptr);
  EXPECT_EQ(*original_intent, *converted_intent);

  // Test intent with all params (except for files) filled in at once.
  // `files` param requires profile which is null in this unit test.
  original_intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                                   GURL("www.google.com"));
  original_intent->share_text = "text";
  original_intent->share_title = "title";
  original_intent->activity_name = "com.android.vending.AssetBrowserActivity";
  original_intent->data = "geo:0,0?q=1600%20amphitheatre%20parkway";
  original_intent->ui_bypassed = true;
  original_intent->extras = base::flat_map<std::string, std::string>{
      {"android.intent.extra.TESTING", "testing"}};
  crosapi_intent =
      apps_util::ConvertAppServiceToCrosapiIntent(original_intent, nullptr);
  converted_intent =
      apps_util::CreateAppServiceIntentFromCrosapi(crosapi_intent, nullptr);
  EXPECT_EQ(*original_intent, *converted_intent);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
class IntentUtilsFileTest : public ::testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing_profile");

    // kFileSystemTypeLocal versus kFileSystemTypeArcContent means that the
    // second one needs to go through Fusebox, as its
    // FileSystemURL::TypeImpliesPathIsReal() returns false.
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            mount_name_local_, storage::FileSystemType::kFileSystemTypeLocal,
            storage::FileSystemMountOption(), base::FilePath(fs_root_local_)));
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            mount_name_arc_, storage::kFileSystemTypeArcContent,
            storage::FileSystemMountOption(), base::FilePath(fs_root_arc_)));
  }

  void TearDown() override {
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
            mount_name_arc_));
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
            mount_name_local_));
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
  const std::string mount_name_arc_ = "TestMountNameArc";
  const std::string mount_name_local_ = "TestMountNameLocal";
  const std::string fs_root_arc_ = "/fake/android/content/path";
  const std::string fs_root_local_ = "/path/to/test/filesystemroot";

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
};

class IntentUtilsFileSystemSchemeTest
    : public IntentUtilsFileTest,
      public ::testing::WithParamInterface<storage::FileSystemType> {};

TEST_P(IntentUtilsFileSystemSchemeTest, ConvertFileSystemScheme) {
  constexpr char fusebox_subdir[] = "my_subdir";
  constexpr bool read_only = false;
  fusebox::Server fusebox_server(nullptr);
  fusebox_server.RegisterFSURLPrefix(
      fusebox_subdir,
      base::StrCat({url::kFileSystemScheme, ":",
                    GetFileManagerOrigin().Serialize(), storage::kExternalDir,
                    "/", mount_name_arc_}),
      read_only);

  base::FilePath in_path;
  base::FilePath out_path;
  switch (GetParam()) {
    case storage::kFileSystemTypeLocal:
      in_path = base::FilePath(storage::kExternalDir).Append(mount_name_local_);
      out_path = base::FilePath(fs_root_local_);
      break;
    case storage::kFileSystemTypeArcContent:
      in_path = base::FilePath(storage::kExternalDir).Append(mount_name_arc_);
      out_path = base::FilePath(file_manager::util::kFuseBoxMediaPath)
                     .Append(fusebox_subdir);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  auto app_service_intent = std::make_unique<apps::Intent>("action");
  app_service_intent->mime_type = "*/*";
  const std::string relative_path = "Documents/foo.txt";
  const std::string mime_type = "text/plain";
  auto url = ToGURL(in_path, relative_path);
  EXPECT_TRUE(url.SchemeIsFileSystem());
  app_service_intent->files = std::vector<apps::IntentFilePtr>{};
  auto file = std::make_unique<apps::IntentFile>(url);
  file->mime_type = mime_type;
  app_service_intent->files.push_back(std::move(file));
  auto crosapi_intent = apps_util::ConvertAppServiceToCrosapiIntent(
      app_service_intent, GetProfile());
  EXPECT_EQ(app_service_intent->action, crosapi_intent->action);
  EXPECT_EQ(app_service_intent->mime_type, crosapi_intent->mime_type);
  ASSERT_TRUE(crosapi_intent->files.has_value());
  ASSERT_EQ(crosapi_intent->files.value().size(), 1U);
  EXPECT_EQ(crosapi_intent->files.value()[0]->file_path,
            out_path.Append(base::FilePath(relative_path)));
  EXPECT_EQ(crosapi_intent->files.value()[0]->mime_type, mime_type);
}

INSTANTIATE_TEST_SUITE_P(
    IntentUtilsFileSystemScheme,
    IntentUtilsFileSystemSchemeTest,
    testing::ValuesIn({storage::kFileSystemTypeLocal,
                       storage::kFileSystemTypeArcContent}));

TEST_F(IntentUtilsFileTest, ConvertFileScheme) {
  auto app_service_intent = std::make_unique<apps::Intent>("action");
  app_service_intent->mime_type = "*/*";
  base::FilePath path("/path/to/document.txt");
  const std::string mime_type = "text/plain";
  auto url = net::FilePathToFileURL(path);
  EXPECT_TRUE(url.SchemeIsFile());
  app_service_intent->files = std::vector<apps::IntentFilePtr>{};
  auto file = std::make_unique<apps::IntentFile>(url);
  file->mime_type = mime_type;
  app_service_intent->files.push_back(std::move(file));
  auto crosapi_intent = apps_util::ConvertAppServiceToCrosapiIntent(
      app_service_intent, GetProfile());
  EXPECT_EQ(app_service_intent->action, crosapi_intent->action);
  EXPECT_EQ(app_service_intent->mime_type, crosapi_intent->mime_type);
  ASSERT_TRUE(crosapi_intent->files.has_value());
  ASSERT_EQ(crosapi_intent->files.value().size(), 1U);
  EXPECT_EQ(crosapi_intent->files.value()[0]->file_path, path);
  EXPECT_EQ(crosapi_intent->files.value()[0]->mime_type, mime_type);
}

TEST_F(IntentUtilsFileTest, CrosapiIntentToAppService) {
  const std::string path = "Documents/foo.txt";
  std::vector<base::FilePath> file_paths;
  file_paths.push_back(base::FilePath(fs_root_local_).Append(path));
  auto crosapi_intent =
      apps_util::CreateCrosapiIntentForViewFiles(std::move(file_paths));

  auto app_service_intent = apps_util::CreateAppServiceIntentFromCrosapi(
      crosapi_intent, GetProfile());
  EXPECT_EQ(app_service_intent->action, crosapi_intent->action);
  EXPECT_EQ(app_service_intent->mime_type, crosapi_intent->mime_type);
  ASSERT_TRUE(crosapi_intent->files.has_value());
  ASSERT_EQ(crosapi_intent->files.value().size(), 1U);
  EXPECT_EQ(
      app_service_intent->files[0]->url,
      ToGURL(base::FilePath(storage::kExternalDir).Append(mount_name_local_),
             path));
}
#endif
