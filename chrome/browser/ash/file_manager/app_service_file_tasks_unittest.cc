// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  AppServiceFileTasksTest() = default;
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
    auto resulting_tasks = FindAppServiceTasksImpl(files);
    return resulting_tasks->tasks;
  }

  std::unique_ptr<ResultingTasks> FindAppServiceTasksImpl(
      const std::vector<FakeFile>& files) {
    std::vector<extensions::EntryInfo> entries;
    std::vector<GURL> file_urls;
    std::vector<std::string> dlp_source_urls;
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
      dlp_source_urls.push_back("");
    }

    auto resulting_tasks = std::make_unique<ResultingTasks>();
    file_tasks::FindAppServiceTasks(profile(), entries, file_urls,
                                    dlp_source_urls, &resulting_tasks->tasks);
    // Sort by app ID so we don't rely on ordering.
    base::ranges::sort(
        resulting_tasks->tasks, base::ranges::less(),
        [](const auto& task) { return task.task_descriptor.app_id; });

    return resulting_tasks;
  }

  std::unique_ptr<ResultingTasks> FindAppServiceTasksWithPolicy(
      const std::vector<FakeFile>& files) {
    auto resulting_tasks = FindAppServiceTasksImpl(files);
    ChooseAndSetDefaultTaskFromPolicyPrefs(
        profile(), ConvertFakeFilesToEntryInfos(files), resulting_tasks.get());
    return resulting_tasks;
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
        base::Value::Dict()
            .Set("name", "Baz")
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("app", base::Value::Dict().Set(
                            "background",
                            base::Value::Dict().Set(
                                "scripts",
                                base::Value::List().Append("background.js"))))
            .Set("file_handlers",
                 base::Value::Dict()
                     .Set("any",
                          base::Value::Dict().Set(
                              "extensions",
                              base::Value::List().Append("*").Append("bar")))
                     .Set("image", base::Value::Dict().Set(
                                       "types", base::Value::List().Append(
                                                    "image/*")))));
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
        base::Value::Dict()
            .Set("name", "Foo")
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("app", base::Value::Dict().Set(
                            "background",
                            base::Value::Dict().Set(
                                "scripts",
                                base::Value::List().Append("background.js"))))
            .Set("file_handlers",
                 base::Value::Dict()
                     .Set("any_with_directories",
                          base::Value::Dict()
                              .Set("include_directories", true)
                              .Set("types", base::Value::List().Append("*"))
                              .Set("verb", "open_with"))
                     .Set("html_handler",
                          base::Value::Dict()
                              .Set("title", "Html")
                              .Set("types",
                                   base::Value::List().Append("text/html"))
                              .Set("verb", "open_with"))
                     .Set("plain_text",
                          base::Value::Dict()
                              .Set("title", "Plain")
                              .Set("types",
                                   base::Value::List().Append("text/plain")))
                     .Set("share_plain_text",
                          base::Value::Dict()
                              .Set("title", "Share Plain")
                              .Set("types",
                                   base::Value::List().Append("text/plain"))
                              .Set("verb", "share_with"))
                     .Set("any_pack",
                          base::Value::Dict()
                              .Set("types", base::Value::List().Append("*"))
                              .Set("verb", "pack_with"))
                     .Set("plain_text_add_to",
                          base::Value::Dict()
                              .Set("title", "Plain")
                              .Set("types",
                                   base::Value::List().Append("text/plain"))
                              .Set("verb", "add_to"))));
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
        base::Value::Dict()
            .Set("name", "Fbh")
            .Set("version", "1.0.0")
            .Set("manifest_version", 2)
            .Set("permissions",
                 base::Value::List().Append("fileBrowserHandler"))
            .Set("file_browser_handlers",
                 base::Value::List().Append(
                     base::Value::Dict()
                         .Set("id", "open")
                         .Set("default_title", "open title")
                         .Set("file_filters", base::Value::List().Append(
                                                  "filesystem:*.txt")))));
    fbh_app.SetID(kExtensionId);
    auto filters =
        apps_util::CreateIntentFiltersForExtension(fbh_app.Build().get());
    AddFakeAppWithIntentFilters(kExtensionId, std::move(filters),
                                apps::AppType::kChromeApp, true,
                                app_service_proxy_);
  }

  // Load an extension from the supplied manifest, then add intent filters.
  void LoadExtension(const std::string manifest) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("file handlers").AddJSON(manifest).Build();
    auto filters = apps_util::CreateIntentFiltersForExtension(extension.get());
    AddFakeAppWithIntentFilters(kExtensionId, std::move(filters),
                                apps::AppType::kExtension,
                                /*handles_intents=*/true, app_service_proxy_);
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

  std::vector<extensions::EntryInfo> ConvertFakeFilesToEntryInfos(
      const std::vector<FakeFile>& files) {
    std::vector<extensions::EntryInfo> entries;
    for (const FakeFile& fake_file : files) {
      entries.emplace_back(
          util::GetMyFilesFolderForProfile(profile()).AppendASCII(
              fake_file.file_name),
          fake_file.mime_type, fake_file.is_directory);
    }
    return entries;
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<apps::AppServiceProxy> app_service_proxy_ = nullptr;
  apps::AppServiceTest app_service_test_;
};

// An app which does not handle intents should not be found even if the filters
// match.
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksHandlesIntent) {
  AddFakeWebApp(kAppIdImage, kMimeTypeImage, kFileExtensionImage,
                kActivityLabelImage, false, app_service_proxy_);
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.jpeg", kMimeTypeImage}});
  ASSERT_EQ(0U, tasks.size());
}

// Test that between an image app and text app, the text app can be
// found for an text file entry.
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksText) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksImage) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksMultiple) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksNoTasks) {
  // Find web apps for a "text/plain" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(0U, tasks.size());
}

// Register a text handler and check we get no matches with an image.
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksNoMatchingTask) {
  AddTextApp();
  // Find apps for a "image/jpeg" file.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(0U, tasks.size());
}

// Check we get a match for a text file + web app.
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksText) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksTwoFilesNoMatch) {
  AddTextApp();
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks(
      {{"foo.txt", kMimeTypeText}, {"bar.jpeg", kMimeTypeImage}});
  ASSERT_EQ(0U, tasks.size());
}

// Check we get a match for a text file + text wildcard filter.
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksTextWild) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksTextWildMultiple) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksAllFilesMatchEither) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceChromeAppText) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceChromeAppBar) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceMultiAppType) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceChromeAppImage) {
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

TEST_F(AppServiceFileTasksTest, FindAppServiceChromeAppWithVerbs) {
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

TEST_F(AppServiceFileTasksTest, FindAppServiceChromeAppWithVerbs_Html) {
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

TEST_F(AppServiceFileTasksTest, FindAppServiceChromeAppWithVerbs_Directory) {
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

TEST_F(AppServiceFileTasksTest, FindAppServiceExtension) {
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

TEST_F(AppServiceFileTasksTest, FindAppServiceArcAppWithExtensionMatching) {
  // Create an app with a text file filter.
  std::string package_name = "com.example.xyzViewer";
  std::string activity = "xyzViewerActivity";
  std::string app_id = AddArcAppWithIntentFilter(
      package_name, activity,
      CreateExtensionTypeFileIntentFilter(apps_util::kIntentActionView, "xyz"));
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks({{"foo.xyz"}});
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(app_id, tasks[0].task_descriptor.app_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_TRUE(tasks[0].is_file_extension_match);
}

// Enable MV3 File Handlers.
class AppServiceFileHandlersTest : public AppServiceFileTasksTest {
 public:
  AppServiceFileHandlersTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionWebFileHandlers);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify App Service tasks for extensions with MV3 File Handlers.
TEST_F(AppServiceFileHandlersTest, FindAppServiceExtension) {
  static constexpr char kAction[] = "/open.html";
  const std::string manifest = base::StringPrintf(R"(
    "version": "0.0.1",
    "manifest_version": 3,
    "file_handlers": [
      {
        "name": "Text file",
        "action": "%s",
        "accept": {"text/plain": ".txt"}
      }
    ]
  )",
                                                  kAction);
  LoadExtension(manifest);
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"test.txt", kMimeTypeText}});

  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kExtensionId, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kAction, tasks[0].task_descriptor.action_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler);
  EXPECT_FALSE(tasks[0].is_file_extension_match);
}

TEST_F(AppServiceFileTasksTest, FindAppServiceArcApp) {
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

TEST_F(AppServiceFileTasksTest, FindAppServiceCrostiniApp) {
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
TEST_F(AppServiceFileTasksTest, CheckPathsCanBeShared) {
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

TEST_F(AppServiceFileTasksTest, FindMultipleAppServiceCrostiniApps) {
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
TEST_F(AppServiceFileTasksTest, FindAppServiceCrostiniAppWithExtension) {
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

TEST_F(AppServiceFileTasksTest, FindAppServicePluginVmApp) {
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

TEST_F(AppServiceFileTasksTest, FindMultipleAppServicePluginVmApps) {
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

TEST_F(AppServiceFileTasksTest,
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

TEST_F(AppServiceFileTasksTest, NoPluginVmAppsForFileSelection) {
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

TEST_F(AppServiceFileTasksTest, CrositiniTasksControlledByPolicy) {
  std::string tini_task_name = "chrome://file-manager/?import-crostini-image";
  std::string deb_task_name = "chrome://file-manager/?install-linux-package";
  std::vector<apps::IntentFilterPtr> filters;
  filters.push_back(
      apps_util::MakeFileFilterForView("tini", "tini", "import-tini"));
  filters[0]->activity_name = tini_task_name;
  filters.push_back(
      apps_util::MakeFileFilterForView("deb", "deb", "import-deb"));
  filters[1]->activity_name = deb_task_name;
  AddFakeAppWithIntentFilters(file_manager::kFileManagerSwaAppId,
                              std::move(filters), apps::AppType::kWeb, true,
                              app_service_proxy_);

  std::string file_name = "test.tini";

  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_export_import_ui_allowed(true);
  std::vector<FullTaskDescriptor> tasks = FindAppServiceTasks({{file_name}});

  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(file_manager::kFileManagerSwaAppId,
            tasks[0].task_descriptor.app_id);
  EXPECT_EQ(tini_task_name, tasks[0].task_descriptor.action_id);

  crostini_features.set_export_import_ui_allowed(false);
  tasks = FindAppServiceTasks({{file_name}});

  ASSERT_EQ(0U, tasks.size());

  file_name = "test.deb";

  crostini_features.set_root_access_allowed(true);
  tasks = FindAppServiceTasks({{file_name}});

  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(file_manager::kFileManagerSwaAppId,
            tasks[0].task_descriptor.app_id);
  EXPECT_EQ(deb_task_name, tasks[0].task_descriptor.action_id);

  crostini_features.set_root_access_allowed(false);
  tasks = FindAppServiceTasks({{file_name}});

  ASSERT_EQ(0U, tasks.size());
}

// Tests applying policies when listing tasks.
class AppServiceFileTasksPolicyTest : public AppServiceFileTasksTest {
 protected:
  class MockFilesController : public policy::DlpFilesControllerAsh {
   public:
    explicit MockFilesController(const policy::DlpRulesManager& rules_manager,
                                 Profile* profile)
        : DlpFilesControllerAsh(rules_manager, profile) {}
    ~MockFilesController() override = default;

    MOCK_METHOD(bool,
                IsLaunchBlocked,
                (const apps::AppUpdate&, const apps::IntentPtr&),
                (override));
  };

  AppServiceFileTasksPolicyTest() = default;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>(
            Profile::FromBrowserContext(context));
    rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  void SetUp() override {
    AppServiceFileTasksTest::SetUp();

    AccountId account_id =
        AccountId::FromUserEmailGaiaId("test@example.com", "12345");
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, /*is_affiliated=*/false,
            user_manager::UserType::kRegular, profile_.get());
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    fake_user_manager_->SimulateUserProfileLoad(account_id);

    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(&AppServiceFileTasksPolicyTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());

    ON_CALL(*rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));
    mock_files_controller_ =
        std::make_unique<MockFilesController>(*rules_manager_, profile_.get());
    ON_CALL(*rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(mock_files_controller_.get()));
  }

  void TearDown() override { fake_user_manager_.Reset(); }

  raw_ptr<policy::MockDlpRulesManager> rules_manager_ = nullptr;
  std::unique_ptr<MockFilesController> mock_files_controller_ = nullptr;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
};

// Test that out of two apps, one can be blocked by DLP and the other allowed.
TEST_F(AppServiceFileTasksPolicyTest, FindAppServiceFileTasksText_DlpChecked) {
  EXPECT_CALL(*mock_files_controller_.get(), IsLaunchBlocked)
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));

  AddTextApp();
  AddAnyApp();
  // Find apps for a "text/plain" file. First app shouldn't be blocked, but the
  // second one yes.
  std::vector<FullTaskDescriptor> tasks =
      FindAppServiceTasks({{"foo.txt", kMimeTypeText}});
  ASSERT_EQ(2U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
  EXPECT_FALSE(tasks[0].is_dlp_blocked);
  EXPECT_EQ(kAppIdAny, tasks[1].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelAny, tasks[1].task_title);
  EXPECT_TRUE(tasks[1].is_dlp_blocked);
}

}  // namespace file_tasks
}  // namespace file_manager.
