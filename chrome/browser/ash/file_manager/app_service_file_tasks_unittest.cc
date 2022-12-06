// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/strings/escape.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "extensions/common/extension_builder.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {
const char kAppIdText[] = "abcdefg";
const char kAppIdImage[] = "gfedcba";
const char kAppIdAny[] = "hijklmn";
const char kChromeAppId[] = "chromeappid";
const char kChromeAppWithVerbsId[] = "chromeappwithverbsid";
const char kExtensionId[] = "extensionid";
const char kAppIdTextWild[] = "zxcvbn";
const char kMimeTypeText[] = "text/plain";
const char kMimeTypeImage[] = "image/jpeg";
const char kMimeTypeHtml[] = "text/html";
const char kMimeTypeAny[] = "*/*";
const char kMimeTypeTextWild[] = "text/*";
const char kFileExtensionText[] = "txt";
const char kFileExtensionImage[] = "jpeg";
const char kFileExtensionAny[] = "fake";
const char kActivityLabelText[] = "some_text_activity";
const char kActivityLabelImage[] = "some_image_activity";
const char kActivityLabelAny[] = "some_any_file";
const char kActivityLabelTextWild[] = "some_text_wild_file";

}  // namespace

namespace file_manager {
namespace file_tasks {
using test::AddFakeAppWithIntentFilters;
using test::AddFakeWebApp;

class AppServiceFileTasksTest : public testing::Test {
 protected:
  AppServiceFileTasksTest() {}
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_service_test_.SetUp(profile_.get());
    app_service_proxy_ =
        apps::AppServiceProxyFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(app_service_proxy_);
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        util::GetDownloadsMountPointName(profile_.get()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        util::GetMyFilesFolderForProfile(profile_.get()));
  }

  Profile* profile() { return profile_.get(); }

  struct FakeFile {
    std::string file_name;
    std::string mime_type;
    bool is_directory = false;
    GURL file_url;
  };

  GURL test_url(const std::string& file_name) {
    GURL url =
        GURL("filesystem:chrome-extension://id/external/" +
             base::EscapeUrlEncodedData(
                 util::GetDownloadsMountPointName(profile()) + "/" + file_name,
                 /*use_plus=*/false));
    EXPECT_TRUE(url.is_valid());
    return url;
  }

  std::vector<FullTaskDescriptor> FindAppServiceTasks(
      const std::vector<FakeFile>& files) {
    std::vector<extensions::EntryInfo> entries;
    std::vector<GURL> file_urls;
    for (const FakeFile& fake_file : files) {
      entries.emplace_back(
          util::GetMyFilesFolderForProfile(profile()).AppendASCII(
              fake_file.file_name),
          fake_file.mime_type, fake_file.is_directory);
      if (fake_file.file_url.is_empty()) {
        file_urls.push_back(test_url(fake_file.file_name));
      } else {
        file_urls.push_back(fake_file.file_url);
      }
    }

    std::vector<FullTaskDescriptor> tasks;
    file_tasks::FindAppServiceTasks(profile(), entries, file_urls, &tasks);
    // Sort by app ID so we don't rely on ordering.
    std::sort(
        tasks.begin(), tasks.end(), [](const auto& left, const auto& right) {
          return left.task_descriptor.app_id < right.task_descriptor.app_id;
        });
    return tasks;
  }

  void AddTextApp() {
    AddFakeWebApp(kAppIdText, kMimeTypeText, kFileExtensionText,
                  kActivityLabelText, true, app_service_proxy_);
  }

  void AddImageApp() {
    AddFakeWebApp(kAppIdImage, kMimeTypeImage, kFileExtensionImage,
                  kActivityLabelImage, true, app_service_proxy_);
  }

  void AddTextWildApp() {
    AddFakeWebApp(kAppIdTextWild, kMimeTypeTextWild, kFileExtensionAny,
                  kActivityLabelTextWild, true, app_service_proxy_);
  }

  void AddAnyApp() {
    AddFakeWebApp(kAppIdAny, kMimeTypeAny, kFileExtensionAny, kActivityLabelAny,
                  true, app_service_proxy_);
  }

  // Provides file handlers for all extensions and images.
  void AddChromeApp() {
    extensions::ExtensionBuilder baz_app;
    baz_app.SetManifest(
        extensions::DictionaryBuilder()
            .Set("name", "Baz")
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("app",
                 extensions::DictionaryBuilder()
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
                    .Set("any", extensions::DictionaryBuilder()
                                    .Set("extensions", extensions::ListBuilder()
                                                           .Append("*")
                                                           .Append("bar")
                                                           .Build())
                                    .Build())
                    .Set("image", extensions::DictionaryBuilder()
                                      .Set("types", extensions::ListBuilder()
                                                        .Append("image/*")
                                                        .Build())
                                      .Build())
                    .Build())
            .Build());
    baz_app.SetID(kChromeAppId);
    auto filters =
        apps_util::CreateIntentFiltersForChromeApp(baz_app.Build().get());
    AddFakeAppWithIntentFilters(kChromeAppId, std::move(filters),
                                apps::AppType::kChromeApp, true,
                                app_service_proxy_);
  }

  void AddChromeAppWithVerbs() {
    extensions::ExtensionBuilder foo_app;
    foo_app.SetManifest(
        extensions::DictionaryBuilder()
            .Set("name", "Foo")
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("app",
                 extensions::DictionaryBuilder()
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
                    .Set("any_with_directories",
                         extensions::DictionaryBuilder()
                             .Set("include_directories", true)
                             .Set("types",
                                  extensions::ListBuilder().Append("*").Build())
                             .Set("verb", "open_with")
                             .Build())
                    .Set("html_handler",
                         extensions::DictionaryBuilder()
                             .Set("title", "Html")
                             .Set("types", extensions::ListBuilder()
                                               .Append("text/html")
                                               .Build())
                             .Set("verb", "open_with")
                             .Build())
                    .Set("plain_text",
                         extensions::DictionaryBuilder()
                             .Set("title", "Plain")
                             .Set("types", extensions::ListBuilder()
                                               .Append("text/plain")
                                               .Build())
                             .Build())
                    .Set("share_plain_text",
                         extensions::DictionaryBuilder()
                             .Set("title", "Share Plain")
                             .Set("types", extensions::ListBuilder()
                                               .Append("text/plain")
                                               .Build())
                             .Set("verb", "share_with")
                             .Build())
                    .Set("any_pack", extensions::DictionaryBuilder()
                                         .Set("types", extensions::ListBuilder()
                                                           .Append("*")
                                                           .Build())
                                         .Set("verb", "pack_with")
                                         .Build())
                    .Set("plain_text_add_to",
                         extensions::DictionaryBuilder()
                             .Set("title", "Plain")
                             .Set("types", extensions::ListBuilder()
                                               .Append("text/plain")
                                               .Build())
                             .Set("verb", "add_to")
                             .Build())
                    .Build())
            .Build());
    foo_app.SetID(kChromeAppWithVerbsId);
    auto filters =
        apps_util::CreateIntentFiltersForChromeApp(foo_app.Build().get());
    AddFakeAppWithIntentFilters(kChromeAppWithVerbsId, std::move(filters),
                                apps::AppType::kChromeApp, true,
                                app_service_proxy_);
  }

  // Adds file_browser_handler to handle .txt files.
  void AddExtension() {
    extensions::ExtensionBuilder fbh_app;
    fbh_app.SetManifest(
        extensions::DictionaryBuilder()
            .Set("name", "Fbh")
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("permissions",
                 extensions::ListBuilder().Append("fileBrowserHandler").Build())
            .Set("file_browser_handlers",
                 extensions::ListBuilder()
                     .Append(extensions::DictionaryBuilder()
                                 .Set("id", "open")
                                 .Set("default_title", "open title")
                                 .Set("file_filters",
                                      extensions::ListBuilder()
                                          .Append("filesystem:*.txt")
                                          .Build())
                                 .Build())
                     .Build())
            .Build());
    fbh_app.SetID(kExtensionId);
    auto filters =
        apps_util::CreateIntentFiltersForExtension(fbh_app.Build().get());
    AddFakeAppWithIntentFilters(kExtensionId, std::move(filters),
                                apps::AppType::kChromeApp, true,
                                app_service_proxy_);
  }

  apps::IntentFilterPtr CreateMimeTypeFileIntentFilter(std::string action,
                                                       std::string mime_type) {
    auto intent_filter = std::make_unique<apps::IntentFilter>();
    intent_filter->AddSingleValueCondition(apps::ConditionType::kAction, action,
                                           apps::PatternMatchType::kLiteral);
    intent_filter->AddSingleValueCondition(apps::ConditionType::kFile,
                                           mime_type,
                                           apps::PatternMatchType::kMimeType);
    return intent_filter;
  }

  apps::IntentFilterPtr CreateExtensionTypeFileIntentFilter(
      std::string action,
      std::string extension_type) {
    auto intent_filter = std::make_unique<apps::IntentFilter>();
    intent_filter->AddSingleValueCondition(apps::ConditionType::kAction, action,
                                           apps::PatternMatchType::kLiteral);
    intent_filter->AddSingleValueCondition(
        apps::ConditionType::kFile, extension_type,
        apps::PatternMatchType::kFileExtension);
    return intent_filter;
  }

  std::string AddArcAppWithIntentFilter(const std::string& package,
                                        const std::string& activity,
                                        apps::IntentFilterPtr intent_filter) {
    std::string app_id = ArcAppListPrefs::GetAppId(package, activity);
    std::vector<apps::IntentFilterPtr> filters;
    filters.push_back(std::move(intent_filter));
    AddFakeAppWithIntentFilters(app_id, std::move(filters), apps::AppType::kArc,
                                true, app_service_proxy_);
    return app_id;
  }

  void AddGuestOsAppWithIntentFilter(std::string app_id,
                                     apps::AppType app_type,
                                     apps::IntentFilterPtr intent_filter) {
    std::vector<apps::IntentFilterPtr> filters;
    filters.push_back(std::move(intent_filter));
    AddFakeAppWithIntentFilters(app_id, std::move(filters), app_type, true,
                                app_service_proxy_);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  apps::AppServiceProxy* app_service_proxy_ = nullptr;
  apps::AppServiceTest app_service_test_;
};

class AppServiceFileTasksTestEnabled : public AppServiceFileTasksTest {
 public:
  AppServiceFileTasksTestEnabled() {
    feature_list_.InitWithFeatures(
        {blink::features::kFileHandlingAPI,
         ash::features::kArcAndGuestOsFileTasksUseAppService},
        {});
  }
};

class AppServiceFileTasksTestDisabled : public AppServiceFileTasksTest {
 public:
  AppServiceFileTasksTestDisabled() {
    feature_list_.InitWithFeatures(
        {}, {blink::features::kFileHandlingAPI,
             ash::features::kArcAndGuestOsFileTasksUseAppService});
  }
};

// Web Apps should not be able to handle files when kFileHandlingAPI is
// disabled.
TEST_F(AppServiceFileTasksTestDisabled, FindAppServiceFileTasksText) {
  AddTextApp();
  // Find apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(0U, tasks.size());
}

// ARC apps should not be found when kArcAndGuestOsFileTasksUseAppService is
// disabled.
TEST_F(AppServiceFileTasksTestDisabled, FindAppServiceArcApp) {
  std::string text_mime_type = "text/plain";

  // Create an app with a text file filter.
  std::string text_package_name = "com.example.textViewer";
  std::string text_activity = "TextViewerActivity";
  std::string text_app_id = AddArcAppWithIntentFilter(
      text_package_name, text_activity,
      CreateMimeTypeFileIntentFilter(apps_util::kIntentActionView,
                                     text_mime_type));

  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", text_mime_type}});
  ASSERT_EQ(0U, tasks.size());
}

// Crostini apps should not be found when kArcAndGuestOsFileTasksUseAppService
// is disabled.
TEST_F(AppServiceFileTasksTestDisabled, FindAppServiceCrostiniApp) {
  std::string text_mime_type = "text/plain";
  std::string file_name = "foo.txt";
  std::string text_app_id = "Text app";
  AddGuestOsAppWithIntentFilter(
      text_app_id, apps::AppType::kCrostini,
      CreateMimeTypeFileIntentFilter(apps_util::kIntentActionView,
                                     text_mime_type));

  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{file_name, text_mime_type}});
  ASSERT_EQ(0U, tasks.size());
}

// PluginVm apps should not be found when kArcAndGuestOsFileTasksUseAppService
// is disabled.
TEST_F(AppServiceFileTasksTestDisabled, FindAppServicePluginVmApp) {
  std::string file_name = "foo.txt";
  std::string app_id = "Text app";
  AddGuestOsAppWithIntentFilter(
      app_id, apps::AppType::kCrostini,
      CreateExtensionTypeFileIntentFilter(apps_util::kIntentActionView, "txt"));

  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{file_name, kMimeTypeText}});
  ASSERT_EQ(0U, tasks.size());
}

// An app which does not handle intents should not be found even if the filters
// match.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceFileTasksHandlesIntent) {
  AddFakeWebApp(kAppIdImage, kMimeTypeImage, kFileExtensionImage,
                kActivityLabelImage, false, app_service_proxy_);
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.jpeg", kMimeTypeImage}});
  ASSERT_EQ(0U, tasks.size());
}

// Test that between an image app and text app, the text app can be
// found for an text file entry.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceFileTasksText) {
  AddTextApp();
  AddImageApp();
  // Find apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
}

// Test that between an image app and text app, the image app can be
// found for an image file entry.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceFileTasksImage) {
  AddTextApp();
  AddImageApp();
  // Find apps for a "image/jpeg" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdImage, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelImage, tasks[0].task_title);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
}

// Test that between an image app, text app and an app that can handle every
// file, the app that can handle every file can be found for an image file entry
// and text file entry.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceFileTasksMultiple) {
  AddTextApp();
  AddImageApp();
  AddAnyApp();
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks(
      {{"foo.txt", kMimeTypeText}, {"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdAny, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelAny, tasks[0].task_title);
  EXPECT_TRUE(tasks[0].is_generic_file_handler);
}

// Don't register any apps and check that we get no matches.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceWebFileTasksNoTasks) {
  // Find web apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(0U, tasks.size());
}

// Register a text handler and check we get no matches with an image.
TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceWebFileTasksNoMatchingTask) {
  AddTextApp();
  // Find apps for a "image/jpeg" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(0U, tasks.size());
}

// Check we get a match for a text file + web app.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceWebFileTasksText) {
  AddTextApp();
  // Find web apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
}

// Check that a web app that only handles text does not match when we have both
// a text file and an image.
TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceWebFileTasksTwoFilesNoMatch) {
  AddTextApp();
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks(
      {{"foo.txt", kMimeTypeText}, {"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(0U, tasks.size());
}

// Check we get a match for a text file + text wildcard filter.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceWebFileTasksTextWild) {
  AddTextWildApp();
  AddTextApp();
  // Find web apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(2U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
  EXPECT_EQ(kAppIdTextWild, tasks[1].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelTextWild, tasks[1].task_title);
}

// Check we get a match for a text file and HTML file + text wildcard filter.
TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceWebFileTasksTextWildMultiple) {
  AddTextWildApp();
  AddTextApp();   // Should not be matched.
  AddImageApp();  // Should not be matched.
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks(
      {{"foo.txt", kMimeTypeText}, {"bar.html", kMimeTypeHtml}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdTextWild, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelTextWild, tasks[0].task_title);
}

// An edge case where we have one file that matches the mime type but not the
// file extension, and another file that matches the file extension but not the
// mime type. This should still match the handler.
TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceWebFileTasksAllFilesMatchEither) {
  AddTextApp();

  // First check that each file alone matches the text app.
  std::vector<FullTaskDescriptor> tasksFoo =
      FindAppServiceTasks({{"foo.txt", "text/plane"}});
  ASSERT_EQ(1U, tasksFoo.size());
  std::vector<FullTaskDescriptor> tasksBar =
      FindAppServiceTasks({{"bar.text", kMimeTypeText}});
  ASSERT_EQ(1U, tasksFoo.size());

  // Now check that both together match.
  std::vector<FullTaskDescriptor> tasksBoth = FindAppServiceTasks(
      {{"foo.txt", "text/plane"}, {"bar.text", kMimeTypeText}});
  ASSERT_EQ(1U, tasksBoth.size());
  EXPECT_EQ(kAppIdText, tasksBoth[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasksBoth[0].task_title);
}

// Check that Baz's ".*" handler, which is generic, is matched.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppText) {
  AddChromeApp();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("any", tasks[0].task_descriptor.action_id);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, tasks[0].task_descriptor.task_type);
  EXPECT_EQ("Baz", tasks[0].task_title);
  EXPECT_TRUE(tasks[0].is_generic_file_handler);
  EXPECT_TRUE(tasks[0].is_file_extension_match);
}

// File extension matches with bar, but there is a generic * type as well,
// so the overall match should still be generic.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppBar) {
  AddChromeApp();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.bar", kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("any", tasks[0].task_descriptor.action_id);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, tasks[0].task_descriptor.task_type);
  EXPECT_EQ("Baz", tasks[0].task_title);
  EXPECT_TRUE(tasks[0].is_generic_file_handler);
  EXPECT_TRUE(tasks[0].is_file_extension_match);
}

// Check that we can get web apps and Chrome apps in the same call.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceMultiAppType) {
  AddTextApp();
  AddChromeApp();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(2U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
  EXPECT_EQ(TASK_TYPE_WEB_APP, tasks[0].task_descriptor.task_type);
  EXPECT_EQ(kChromeAppId, tasks[1].task_descriptor.app_id);
  EXPECT_EQ("Baz", tasks[1].task_title);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, tasks[1].task_descriptor.task_type);
}

// Check that Baz's "image/*" handler is picked because it is not generic,
// because it matches the mime type directly, even though there is an earlier
// generic handler.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppImage) {
  AddChromeApp();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("image", tasks[0].task_descriptor.action_id);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, tasks[0].task_descriptor.task_type);
  EXPECT_EQ("Baz", tasks[0].task_title);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppWithVerbs) {
  AddChromeAppWithVerbs();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});

  // We expect that all non-"open_with" handlers are ignored, and that we
  // only get one open_with handler.
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppWithVerbsId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("Foo", tasks[0].task_title);
  EXPECT_EQ("plain_text", tasks[0].task_descriptor.action_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceChromeAppWithVerbs_Html) {
  AddChromeAppWithVerbs();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.html", kMimeTypeHtml}});

  // Check that we get the non-generic handler which appears later in the
  // manifest.
  EXPECT_EQ(kChromeAppWithVerbsId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("Foo", tasks[0].task_title);
  EXPECT_EQ("html_handler", tasks[0].task_descriptor.action_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServiceChromeAppWithVerbs_Directory) {
  AddChromeAppWithVerbs();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"dir", "", true}});

  // Only one handler handles directories.
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kChromeAppWithVerbsId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("Foo", tasks[0].task_title);
  EXPECT_EQ("any_with_directories", tasks[0].task_descriptor.action_id);
  EXPECT_TRUE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceExtension) {
  AddExtension();
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});

  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kExtensionId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ("open title", tasks[0].task_title);
  EXPECT_EQ("open", tasks[0].task_descriptor.action_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceArcApp) {
  std::string text_mime_type = "text/plain";
  std::string image_mime_type = "image/jpeg";

  // Create an app with a text file filter.
  std::string text_package_name = "com.example.textViewer";
  std::string text_activity = "TextViewerActivity";
  std::string text_app_id = AddArcAppWithIntentFilter(
      text_package_name, text_activity,
      CreateMimeTypeFileIntentFilter(apps_util::kIntentActionView,
                                     text_mime_type));

  // Create an app with an image file filter.
  std::string image_package_name = "com.example.imageViewer";
  std::string image_activity = "ImageViewerActivity";
  std::string image_app_id = AddArcAppWithIntentFilter(
      image_package_name, image_activity,
      CreateMimeTypeFileIntentFilter(apps_util::kIntentActionView,
                                     image_mime_type));

  // Check if only the text ARC app appears as a result.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", text_mime_type}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(text_app_id, tasks[0].task_descriptor.app_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceCrostiniApp) {
  std::string file_name = "foo.txt";
  std::string text_app_id = "Text app";
  AddGuestOsAppWithIntentFilter(
      text_app_id, apps::AppType::kCrostini,
      CreateMimeTypeFileIntentFilter(apps_util::kIntentActionView,
                                     kMimeTypeText));

  // Check if the text Crostini app is returned.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{file_name, kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(text_app_id, tasks[0].task_descriptor.app_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
}

// Checks that we can detect when the file paths can/ can't be shared for
// Crostini and PluginVm.
TEST_F(AppServiceFileTasksTestEnabled, CheckPathsCanBeShared) {
  std::string file_name = "foo.txt";
  std::string text_app_id = "Text app";
  AddGuestOsAppWithIntentFilter(
      text_app_id, apps::AppType::kCrostini,
      CreateMimeTypeFileIntentFilter(apps_util::kIntentActionView,
                                     kMimeTypeText));

  // Possible to share path.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{file_name, kMimeTypeText}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(text_app_id, tasks[0].task_descriptor.app_id);

  // Should not be possible to share path.
  GURL invalid_url = GURL("broken:url");
  tasks =
      FindAppServiceTasks({{file_name, kMimeTypeText, /*is_directory=*/false,
                            /*file_url=*/invalid_url}});
  ASSERT_EQ(0U, tasks.size());
}

TEST_F(AppServiceFileTasksTestEnabled, FindMultipleAppServiceCrostiniApps) {
  std::string file_name = "foo.txt";
  std::string app_id_1 = "Text app 1";
  std::string app_id_2 = "Text app 2";
  AddGuestOsAppWithIntentFilter(
      app_id_1, apps::AppType::kCrostini,
      CreateMimeTypeFileIntentFilter(apps_util::kIntentActionView,
                                     kMimeTypeText));
  AddGuestOsAppWithIntentFilter(
      app_id_2, apps::AppType::kCrostini,
      CreateMimeTypeFileIntentFilter(apps_util::kIntentActionView,
                                     kMimeTypeText));

  // Check if both Crostini apps are returned.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{file_name, kMimeTypeText}});
  ASSERT_EQ(2U, tasks.size());

  EXPECT_EQ(app_id_1, tasks[0].task_descriptor.app_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);

  EXPECT_EQ(app_id_2, tasks[1].task_descriptor.app_id);
  EXPECT_FALSE(tasks[1].is_generic_file_handler);
  EXPECT_FALSE(tasks[1].is_file_extension_match);
}

// When we encounter a file with an unknown mime-type (i.e.
// application/octet-stream), we rely on matching with the extension type. Check
// whether extension matching works for Crostini.
TEST_F(AppServiceFileTasksTestEnabled, FindAppServiceCrostiniAppWithExtension) {
  std::string extension = "randomExtension";
  std::string mime_type = "test/randomMimeType";
  std::string file_name = "foo." + extension;
  std::string app_id = "App";

  auto intent_filter =
      apps_util::CreateFileFilter({apps_util::kIntentActionView}, {mime_type},
                                  {extension}, "open-with", false);
  AddGuestOsAppWithIntentFilter(app_id, apps::AppType::kCrostini,
                                std::move(intent_filter));

  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{file_name, "application/octet-stream"}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(app_id, tasks[0].task_descriptor.app_id);
}

TEST_F(AppServiceFileTasksTestEnabled, FindAppServicePluginVmApp) {
  std::string file_ext = "txt";
  std::string file_name = "foo." + file_ext;
  std::string text_app_id = "Text app";
  AddGuestOsAppWithIntentFilter(text_app_id, apps::AppType::kPluginVm,
                                CreateExtensionTypeFileIntentFilter(
                                    apps_util::kIntentActionView, file_ext));

  // Check if the text PluginVm app is returned.
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks({{file_name}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(text_app_id, tasks[0].task_descriptor.app_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_TRUE(tasks[0].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTestEnabled, FindMultipleAppServicePluginVmApps) {
  std::string file_ext = "txt";
  std::string file_name = "foo." + file_ext;
  std::string app_id_1 = "Text app 1";
  std::string app_id_2 = "Text app 2";
  AddGuestOsAppWithIntentFilter(app_id_1, apps::AppType::kPluginVm,
                                CreateExtensionTypeFileIntentFilter(
                                    apps_util::kIntentActionView, file_ext));
  AddGuestOsAppWithIntentFilter(app_id_2, apps::AppType::kPluginVm,
                                CreateExtensionTypeFileIntentFilter(
                                    apps_util::kIntentActionView, file_ext));

  // Check if both PluginVm apps are returned.
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks({{file_name}});
  ASSERT_EQ(2U, tasks.size());

  EXPECT_EQ(app_id_1, tasks[0].task_descriptor.app_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_TRUE(tasks[0].is_file_extension_match);

  EXPECT_EQ(app_id_2, tasks[1].task_descriptor.app_id);
  EXPECT_FALSE(tasks[1].is_generic_file_handler);
  EXPECT_TRUE(tasks[1].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTestEnabled,
       FindAppServicePluginVmApp_IgnoringExtensionCase) {
  std::string file_ext = "Txt";
  std::string file_name = "foo.txT";
  std::string text_app_id = "Text app";
  AddGuestOsAppWithIntentFilter(text_app_id, apps::AppType::kPluginVm,
                                CreateExtensionTypeFileIntentFilter(
                                    apps_util::kIntentActionView, file_ext));

  // Check if the text PluginVm app is returned.
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks({{file_name}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(text_app_id, tasks[0].task_descriptor.app_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_TRUE(tasks[0].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTestEnabled, NoPluginVmAppsForFileSelection) {
  std::string image_file_name = "foo.jpeg";
  std::string image_app_id = "Image app";
  std::string text_file_name = "foo.txt";
  std::string text_app_id = "Text app";

  // Add a text-only app and an image-only app.
  AddGuestOsAppWithIntentFilter(
      text_app_id, apps::AppType::kPluginVm,
      CreateExtensionTypeFileIntentFilter(apps_util::kIntentActionView, "txt"));
  AddGuestOsAppWithIntentFilter(image_app_id, apps::AppType::kPluginVm,
                                CreateExtensionTypeFileIntentFilter(
                                    apps_util::kIntentActionView, "jpeg"));

  // Find an app that can open both the text and image file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{text_file_name}, {image_file_name}});

  // There shouldn't be any apps available.
  ASSERT_EQ(0U, tasks.size());
}

}  // namespace file_tasks
}  // namespace file_manager.
