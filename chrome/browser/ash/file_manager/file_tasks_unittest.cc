// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_tasks.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "google_apis/drive/drive_api_parser.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace file_manager::file_tasks {

TEST(FileManagerFileTasksTest, FullTaskDescriptor_WithIconAndDefault) {
  FullTaskDescriptor full_descriptor(
      TaskDescriptor("app-id", TASK_TYPE_FILE_BROWSER_HANDLER, "action-id"),
      "task title", GURL("http://example.com/icon.png"), true /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);

  EXPECT_EQ("app-id", full_descriptor.task_descriptor.app_id);
  EXPECT_EQ(TaskType::TASK_TYPE_FILE_BROWSER_HANDLER,
            full_descriptor.task_descriptor.task_type);
  EXPECT_EQ("action-id", full_descriptor.task_descriptor.action_id);
  EXPECT_EQ("http://example.com/icon.png", full_descriptor.icon_url.spec());
  EXPECT_EQ("task title", full_descriptor.task_title);
  EXPECT_TRUE(full_descriptor.is_default);
}

TEST(FileManagerFileTasksTest, MakeTaskID) {
  EXPECT_EQ("app-id|file|action-id",
            MakeTaskID("app-id", TASK_TYPE_FILE_BROWSER_HANDLER, "action-id"));
  EXPECT_EQ("app-id|app|action-id",
            MakeTaskID("app-id", TASK_TYPE_FILE_HANDLER, "action-id"));
}

TEST(FileManagerFileTasksTest, TaskDescriptorToId) {
  EXPECT_EQ("app-id|file|action-id",
            TaskDescriptorToId(TaskDescriptor(
                "app-id", TASK_TYPE_FILE_BROWSER_HANDLER, "action-id")));
}

TEST(FileManagerFileTasksTest, ParseTaskID_FileBrowserHandler) {
  TaskDescriptor task;
  EXPECT_TRUE(ParseTaskID("app-id|file|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ(TASK_TYPE_FILE_BROWSER_HANDLER, task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_FileHandler) {
  TaskDescriptor task;
  EXPECT_TRUE(ParseTaskID("app-id|app|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_Legacy) {
  TaskDescriptor task;
  // A legacy task ID only has two parts. The task type should be
  // TASK_TYPE_FILE_BROWSER_HANDLER.
  EXPECT_TRUE(ParseTaskID("app-id|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ(TASK_TYPE_FILE_BROWSER_HANDLER, task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_Invalid) {
  TaskDescriptor task;
  EXPECT_FALSE(ParseTaskID("invalid", &task));
}

TEST(FileManagerFileTasksTest, ParseTaskID_UnknownTaskType) {
  TaskDescriptor task;
  EXPECT_FALSE(ParseTaskID("app-id|unknown|action-id", &task));
}

TEST(FileManagerFileTasksTest, BaseContainsFindsTaskDescriptors) {
  // Create several task descriptors that each have one field with the
  // "smallest" value in that category. i.e. task_1 has the "smallest" app_id
  // value, task_2 has the "smallest" task type value, and task_3 has the
  // "smallest" action_id value.
  TaskDescriptor task_1("a", TASK_TYPE_ARC_APP, "other");
  TaskDescriptor task_2("b", TASK_TYPE_FILE_BROWSER_HANDLER, "view");
  TaskDescriptor task_3("c", TASK_TYPE_FILE_HANDLER, "edit");

  std::set<TaskDescriptor> tasks;
  tasks.insert(task_1);
  tasks.insert(task_2);
  tasks.insert(task_3);

  ASSERT_TRUE(base::Contains(tasks, task_1));
  ASSERT_TRUE(base::Contains(tasks, task_2));
  ASSERT_TRUE(base::Contains(tasks, task_3));
}

TEST(FileManagerFileTasksTest, EqualTaskDescriptors) {
  TaskDescriptor task_1("a", TASK_TYPE_FILE_HANDLER, "view");
  TaskDescriptor task_2("a", TASK_TYPE_FILE_HANDLER, "view");

  ASSERT_EQ(task_1, task_2);
}

TEST(FileManagerFileTasksTest, NotEqualAppIdInTaskDescriptors) {
  TaskDescriptor task_1("a", TASK_TYPE_FILE_HANDLER, "view");
  TaskDescriptor task_2("b", TASK_TYPE_FILE_HANDLER, "view");

  ASSERT_NE(task_1, task_2);
}

TEST(FileManagerFileTasksTest, NotEqualTaskTypeInTaskDescriptors) {
  TaskDescriptor task_1("a", TASK_TYPE_FILE_HANDLER, "view");
  TaskDescriptor task_2("a", TASK_TYPE_FILE_BROWSER_HANDLER, "view");

  ASSERT_NE(task_1, task_2);
}

TEST(FileManagerFileTasksTest, NotEqualActionIdInTaskDescriptors) {
  TaskDescriptor task_1("a", TASK_TYPE_FILE_HANDLER, "view");
  TaskDescriptor task_2("a", TASK_TYPE_FILE_HANDLER, "edit");

  ASSERT_NE(task_1, task_2);
}

// Test FileHandlerIsEnabled which returns whether a file handler should be
// used.
TEST(FileManagerFileTasksTest, FileHandlerIsEnabled) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile test_profile;
  crostini::FakeCrostiniFeatures crostini_features;

  const std::string test_id = "test";

  crostini_features.set_export_import_ui_allowed(true);
  EXPECT_TRUE(
      FileHandlerIsEnabled(&test_profile, kFileManagerSwaAppId,
                           "chrome://file-manager/?import-crostini-image"));
  EXPECT_TRUE(
      FileHandlerIsEnabled(&test_profile, kFileManagerSwaAppId, test_id));

  crostini_features.set_export_import_ui_allowed(false);
  EXPECT_FALSE(
      FileHandlerIsEnabled(&test_profile, kFileManagerSwaAppId,
                           "chrome://file-manager/?import-crostini-image"));
  EXPECT_TRUE(
      FileHandlerIsEnabled(&test_profile, kFileManagerSwaAppId, test_id));

  crostini_features.set_root_access_allowed(true);
  EXPECT_TRUE(
      FileHandlerIsEnabled(&test_profile, kFileManagerSwaAppId,
                           "chrome://file-manager/?install-linux-package"));
  EXPECT_TRUE(
      FileHandlerIsEnabled(&test_profile, kFileManagerSwaAppId, test_id));

  crostini_features.set_root_access_allowed(false);
  EXPECT_FALSE(
      FileHandlerIsEnabled(&test_profile, kFileManagerSwaAppId,
                           "chrome://file-manager/?install-linux-package"));
  EXPECT_TRUE(
      FileHandlerIsEnabled(&test_profile, kFileManagerSwaAppId, test_id));
}

class FileManagerFileTaskWithAppServiceTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    app_service_test_.SetUp(profile_.get());
    app_service_proxy_ =
        apps::AppServiceProxyFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(app_service_proxy_);
  }

  void AddFakeAppToAppService(const std::string& app_id,
                              const absl::optional<std::string>& package_name,
                              std::vector<std::string> policy_ids,
                              apps::AppType app_type) {
    auto app = std::make_unique<apps::App>(app_type, app_id);
    app->app_id = app_id;
    app->app_type = app_type;
    app->publisher_id = package_name;
    app->policy_ids = std::move(policy_ids);
    app->readiness = apps::Readiness::kReady;

    std::vector<apps::AppPtr> apps;
    apps.push_back(std::move(app));
    app_service_proxy()->AppRegistryCache().OnApps(
        std::move(apps), app_type, false /* should_notify_initialized */);
  }

  TestingProfile* profile() { return profile_.get(); }
  apps::AppServiceProxy* app_service_proxy() { return app_service_proxy_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<apps::AppServiceProxy, ExperimentalAsh> app_service_proxy_ = nullptr;
  apps::AppServiceTest app_service_test_;
};

using AppIdPolicyIdPair = std::pair<const char*, const char*>;

class FileManagerFileTaskPolicyDefaultHandlersTest
    : public FileManagerFileTaskWithAppServiceTest {
 public:
  void SetUp() override {
    FileManagerFileTaskWithAppServiceTest::SetUp();
    CreateAppsAndTasks();
  }

 protected:
  void UpdateDefaultHandlersPrefs(
      const std::vector<std::pair<std::string, std::string>>& handlers = {}) {
    base::Value::Dict pref_dict;
    for (const auto& [file_extension, policy_id] : handlers) {
      pref_dict.Set(file_extension, policy_id);
    }
    profile()->GetTestingPrefService()->SetDict(
        prefs::kDefaultHandlersForFileExtensions, std::move(pref_dict));
  }

  ResultingTasks* resulting_tasks() { return resulting_tasks_.get(); }
  std::vector<extensions::EntryInfo>& entries() { return entries_; }

  void CheckCorrectPolicyAssignment(const std::string& default_app_id) {
    ASSERT_EQ(resulting_tasks()->policy_default_handler_status,
              PolicyDefaultHandlerStatus::kDefaultHandlerAssignedByPolicy);
    ASSERT_EQ(base::ranges::count_if(resulting_tasks()->tasks, &IsDefaultTask),
              1);
    ASSERT_EQ(base::ranges::find_if(resulting_tasks()->tasks, &IsDefaultTask)
                  ->task_descriptor.app_id,
              default_app_id);
  }

  void CheckConflictingPolicyAssignment() {
    ASSERT_EQ(resulting_tasks()->policy_default_handler_status,
              PolicyDefaultHandlerStatus::kIncorrectAssignment);
    ASSERT_EQ(base::ranges::count_if(resulting_tasks()->tasks, &IsDefaultTask),
              0);
  }

  void CheckNoPolicyAssignment() {
    ASSERT_FALSE(resulting_tasks()->policy_default_handler_status);
    ASSERT_EQ(base::ranges::count_if(resulting_tasks()->tasks, &IsDefaultTask),
              0);
  }

 protected:
  static constexpr char kWebAppId[] = "web-app-id";
  static constexpr char kChromeAppId[] = "chrome-app-id";
  static constexpr char kArcAppId[] = "arc-app-id";
  static constexpr char kNonExistentAppId[] = "null";

  static constexpr char kWebAppUrl[] = "https://web.app";
  static constexpr char kArcAppPackageName[] = "com.package.name";

  static constexpr AppIdPolicyIdPair kAppIdPolicyIdMapping[] = {
      {kWebAppId, kWebAppUrl},
      {kArcAppId, kArcAppPackageName},
      {kChromeAppId, kChromeAppId}};

 private:
  void CreateAppsAndTasks() {
    resulting_tasks_ = std::make_unique<ResultingTasks>();

    std::vector<FullTaskDescriptor>& tasks = resulting_tasks()->tasks;
    for (const auto& [app_id, _] : kAppIdPolicyIdMapping) {
      tasks.emplace_back(
          TaskDescriptor{app_id, TASK_TYPE_FILE_HANDLER, "action-id"},
          /*task_title=*/"Task", GURL("https://example.com/app.png"), false,
          false, false);
    }

    AddFakeAppToAppService(kWebAppId, /*package_name=*/{},
                           /*policy_ids=*/{kWebAppUrl}, apps::AppType::kWeb);
    AddFakeAppToAppService(kChromeAppId, /*package_name=*/{},
                           /*policy_ids=*/{kChromeAppId},
                           apps::AppType::kChromeApp);
    AddFakeAppToAppService(kArcAppId, /*package_name=*/kArcAppPackageName,
                           /*policy_ids=*/{kArcAppPackageName},
                           apps::AppType::kArc);
  }

  static bool IsDefaultTask(const FullTaskDescriptor& ftd) {
    return ftd.is_default;
  }

  std::unique_ptr<ResultingTasks> resulting_tasks_;
  std::vector<extensions::EntryInfo> entries_;
};

// Check that no default tasks are set if no policy is set.
TEST_F(FileManagerFileTaskPolicyDefaultHandlersTest, CheckNoPolicyAssignment) {
  entries().emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"),
                         "text/plain", false);

  UpdateDefaultHandlersPrefs(/*empty*/);
  ASSERT_FALSE(ChooseAndSetDefaultTaskFromPolicyPrefs(profile(), entries(),
                                                      resulting_tasks()));
  CheckNoPolicyAssignment();
}

// Check that a policy set to a non-existent app is ignored.
TEST_F(FileManagerFileTaskPolicyDefaultHandlersTest,
       CheckAssignmentToNonExistentApp) {
  entries().emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"),
                         "text/plain", false);

  UpdateDefaultHandlersPrefs({{".txt", kNonExistentAppId}});
  ASSERT_FALSE(ChooseAndSetDefaultTaskFromPolicyPrefs(profile(), entries(),
                                                      resulting_tasks()));
  CheckNoPolicyAssignment();
}

// Check that assigning different apps to handle different file extensions
// leads to a conflict.
TEST_F(FileManagerFileTaskPolicyDefaultHandlersTest,
       CheckConflictingPolicyAssignment) {
  entries().emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"),
                         "text/plain", false);
  entries().emplace_back(base::FilePath::FromUTF8Unsafe("foo.csv"), "text/csv",
                         false);

  UpdateDefaultHandlersPrefs({{".txt", kWebAppUrl}, {".csv", kChromeAppId}});
  ASSERT_TRUE(ChooseAndSetDefaultTaskFromPolicyPrefs(profile(), entries(),
                                                     resulting_tasks()));
  CheckConflictingPolicyAssignment();
}

// Check that legacy arc app format is parsed correctly.
TEST_F(FileManagerFileTaskPolicyDefaultHandlersTest, LegacyArcAppFormat) {
  resulting_tasks()->tasks.emplace_back(
      TaskDescriptor{"com.legacy.package/intentName", TASK_TYPE_ARC_APP,
                     "view"},
      /*task_title=*/"Task", GURL(), false, false, false);
  entries().emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"),
                         "text/plain", false);

  UpdateDefaultHandlersPrefs({{".txt", "com.legacy.package"}});
  ASSERT_TRUE(ChooseAndSetDefaultTaskFromPolicyPrefs(profile(), entries(),
                                                     resulting_tasks()));
  CheckCorrectPolicyAssignment("com.legacy.package/intentName");
}

class FileManagerFileTaskPolicyDefaultHandlersTestPerAppType
    : public FileManagerFileTaskPolicyDefaultHandlersTest,
      public testing::WithParamInterface<AppIdPolicyIdPair> {
 public:
  // This is required to correctly instantiate TEST_SUITE_P.
  using FileManagerFileTaskPolicyDefaultHandlersTest::kAppIdPolicyIdMapping;
};

// Check that default tasks are set correctly by policy_id.
TEST_P(FileManagerFileTaskPolicyDefaultHandlersTestPerAppType,
       ChooseAndSetDefaultTaskFromPolicyPrefsForSingleFileExtension) {
  entries().emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"),
                         "text/plain", false);

  const auto [app_id, policy_id] = GetParam();
  UpdateDefaultHandlersPrefs({{".txt", policy_id}});
  ASSERT_TRUE(ChooseAndSetDefaultTaskFromPolicyPrefs(profile(), entries(),
                                                     resulting_tasks()));
  CheckCorrectPolicyAssignment(app_id);
}

// Check that default tasks are set correctly by policy_id for multiple
// file_extensions.
TEST_P(FileManagerFileTaskPolicyDefaultHandlersTestPerAppType,
       ChooseAndSetDefaultTaskFromPolicyPrefsForMultipleFileExtensions) {
  entries().emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"),
                         "text/plain", false);
  entries().emplace_back(base::FilePath::FromUTF8Unsafe("foo.csv"), "text/csv",
                         false);

  const auto [app_id, policy_id] = GetParam();
  UpdateDefaultHandlersPrefs({{".txt", policy_id}, {".csv", policy_id}});
  ASSERT_TRUE(ChooseAndSetDefaultTaskFromPolicyPrefs(profile(), entries(),
                                                     resulting_tasks()));
  CheckCorrectPolicyAssignment(app_id);
}

INSTANTIATE_TEST_SUITE_P(
    /**/,
    FileManagerFileTaskPolicyDefaultHandlersTestPerAppType,
    testing::ValuesIn(FileManagerFileTaskPolicyDefaultHandlersTestPerAppType::
                          kAppIdPolicyIdMapping));

class FileManagerFileTaskPreferencesTest
    : public FileManagerFileTaskWithAppServiceTest {
 public:
  // Updates the default task preferences per the given dictionary values.
  // Used for testing ChooseAndSetDefaultTask.
  void UpdateDefaultTaskPreferences(const base::Value::Dict& mime_types,
                                    const base::Value::Dict& suffixes) {
    profile()->GetTestingPrefService()->SetDict(prefs::kDefaultTasksByMimeType,
                                                mime_types.Clone());
    profile()->GetTestingPrefService()->SetDict(prefs::kDefaultTasksBySuffix,
                                                suffixes.Clone());
  }  // namespace file_manager::file_tasks

  const base::Value::Dict& tasks_by_mime_type() {
    return profile()->GetTestingPrefService()->GetDict(
        prefs::kDefaultTasksByMimeType);
  }

  const base::Value::Dict& tasks_by_suffix() {
    return profile()->GetTestingPrefService()->GetDict(
        prefs::kDefaultTasksBySuffix);
  }

  void ClearPrefs() {
    profile()->GetTestingPrefService()->ClearPref(
        prefs::kDefaultTasksByMimeType);
    profile()->GetTestingPrefService()->ClearPref(prefs::kDefaultTasksBySuffix);
  }
};

// Test that the right task is chosen from multiple choices per mime types
// and file extensions.
TEST_F(FileManagerFileTaskPreferencesTest,
       ChooseAndSetDefaultTask_MultipleTasks) {
  // Text.app and Nice.app were found for "foo.txt".
  TaskDescriptor text_app_task("text-app-id", TASK_TYPE_FILE_HANDLER,
                               "action-id");
  TaskDescriptor nice_app_task("nice-app-id", TASK_TYPE_FILE_HANDLER,
                               "action-id");

  auto resulting_tasks = std::make_unique<ResultingTasks>();
  std::vector<FullTaskDescriptor>& tasks = resulting_tasks->tasks;

  tasks.emplace_back(
      text_app_task, "Text.app", GURL("http://example.com/text_app.png"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  tasks.emplace_back(
      nice_app_task, "Nice.app", GURL("http://example.com/nice_app.png"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"), "text/plain",
                       false);

  // None of them should be chosen as default, as nothing is set in the
  // preferences.
  ChooseAndSetDefaultTask(profile(), entries, resulting_tasks.get());
  EXPECT_FALSE(tasks[0].is_default);
  EXPECT_FALSE(tasks[1].is_default);

  // Set Text.app as default for "text/plain" in the preferences.
  base::Value::Dict empty;
  base::Value::Dict mime_types;
  mime_types.Set("text/plain", base::Value(TaskDescriptorToId(text_app_task)));
  UpdateDefaultTaskPreferences(mime_types, empty);

  // Text.app should be chosen as default.
  ChooseAndSetDefaultTask(profile(), entries, resulting_tasks.get());
  EXPECT_TRUE(tasks[0].is_default);
  EXPECT_FALSE(tasks[1].is_default);

  // Change it back to non-default for testing further.
  tasks[0].is_default = false;

  // Clear the preferences and make sure none of them are default.
  UpdateDefaultTaskPreferences(empty, empty);
  ChooseAndSetDefaultTask(profile(), entries, resulting_tasks.get());
  EXPECT_FALSE(tasks[0].is_default);
  EXPECT_FALSE(tasks[1].is_default);

  // Set Nice.app as default for ".txt" in the preferences.
  base::Value::Dict suffixes;
  suffixes.Set(".txt", base::Value(TaskDescriptorToId(nice_app_task)));
  UpdateDefaultTaskPreferences(empty, suffixes);

  // Now Nice.app should be chosen as default.
  ChooseAndSetDefaultTask(profile(), entries, resulting_tasks.get());
  EXPECT_FALSE(tasks[0].is_default);
  EXPECT_TRUE(tasks[1].is_default);
}

// Test that internal file browser handler of the Files app is chosen as
// default even if nothing is set in the preferences.
TEST_F(FileManagerFileTaskPreferencesTest,
       ChooseAndSetDefaultTask_FallbackFileBrowser) {
  // The internal file browser handler of the Files app was found for
  // "foo.txt".
  TaskDescriptor files_app_task(
      kFileManagerAppId, TASK_TYPE_FILE_BROWSER_HANDLER, "view-in-browser");

  auto resulting_tasks = std::make_unique<ResultingTasks>();
  std::vector<FullTaskDescriptor>& tasks = resulting_tasks->tasks;

  tasks.emplace_back(
      files_app_task, "View in browser",
      GURL("http://example.com/some_icon.png"), false /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"), "text/plain",
                       false);

  // The internal file browser handler should be chosen as default, as it's a
  // fallback file browser handler.
  ChooseAndSetDefaultTask(profile(), entries, resulting_tasks.get());
  EXPECT_TRUE(tasks[0].is_default);
}

// Test that Text.app is chosen as default instead of the Files app
// even if nothing is set in the preferences.
TEST_F(FileManagerFileTaskPreferencesTest,
       ChooseAndSetDefaultTask_FallbackTextApp) {
  // Define the browser handler of the Files app for "foo.txt".
  TaskDescriptor files_app_task(
      kFileManagerAppId, TASK_TYPE_FILE_BROWSER_HANDLER, "view-in-browser");
  // Define the text editor app for "foo.txt".
  TaskDescriptor text_app_task(kTextEditorAppId, TASK_TYPE_FILE_HANDLER,
                               "Text");

  auto resulting_tasks = std::make_unique<ResultingTasks>();
  std::vector<FullTaskDescriptor>& tasks = resulting_tasks->tasks;

  tasks.emplace_back(
      files_app_task, "View in browser",
      GURL("http://example.com/some_icon.png"), false /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);
  tasks.emplace_back(
      text_app_task, "Text",
      GURL("chrome://extension-icon/mmfbcljfglbokpmkimbfghdkjmjhdgbg/16/1"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"), "text/plain",
                       false);

  // The text editor app should be chosen as default, as it's a fallback file
  // browser handler.
  ChooseAndSetDefaultTask(profile(), entries, resulting_tasks.get());
  EXPECT_TRUE(tasks[1].is_default);
}

// Test that browser is chosen as default for HTML files instead of the Text
// app even if nothing is set in the preferences.
TEST_F(FileManagerFileTaskPreferencesTest,
       ChooseAndSetDefaultTask_FallbackHtmlTextApp) {
  // Define the browser handler of the Files app for "foo.html".
  TaskDescriptor files_app_task(
      kFileManagerAppId, TASK_TYPE_FILE_BROWSER_HANDLER, "view-in-browser");
  // Define the text editor app for "foo.html".
  TaskDescriptor text_app_task(kTextEditorAppId, TASK_TYPE_FILE_HANDLER,
                               "Text");

  auto resulting_tasks = std::make_unique<ResultingTasks>();
  std::vector<FullTaskDescriptor>& tasks = resulting_tasks->tasks;

  tasks.emplace_back(
      files_app_task, "View in browser",
      GURL("http://example.com/some_icon.png"), false /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);
  tasks.emplace_back(
      text_app_task, "Text",
      GURL("chrome://extension-icon/mmfbcljfglbokpmkimbfghdkjmjhdgbg/16/1"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.html"), "text/html",
                       false);

  // The internal file browser handler should be chosen as default,
  // as it's a fallback file browser handler.
  ChooseAndSetDefaultTask(profile(), entries, resulting_tasks.get());
  EXPECT_TRUE(tasks[0].is_default);
}

// Test that Office Editing is chosen as default even if nothing is set in the
// preferences.
TEST_F(FileManagerFileTaskPreferencesTest,
       ChooseAndSetDefaultTask_FallbackOfficeEditing) {
  // The Office Editing app was found for "slides.pptx".
  TaskDescriptor files_app_task(
      extension_misc::kQuickOfficeComponentExtensionId, TASK_TYPE_FILE_HANDLER,
      "Office Editing for Docs, Sheets & Slides");

  auto resulting_tasks = std::make_unique<ResultingTasks>();
  std::vector<FullTaskDescriptor>& tasks = resulting_tasks->tasks;

  tasks.emplace_back(
      files_app_task, "Office Editing for Docs, Sheets & Slides",
      GURL("chrome://extension-icon/bpmcpldpdmajfigpchkicefoigmkfalc/32/1"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("slides.pptx"), "",
                       false);

  // The Office Editing app should be chosen as default, as it's a fallback
  // file browser handler.
  ChooseAndSetDefaultTask(profile(), entries, resulting_tasks.get());
  EXPECT_TRUE(tasks[0].is_default);
}

// Test that for changes of default app for PDF files, a metric is recorded.
TEST_F(FileManagerFileTaskPreferencesTest,
       UpdateDefaultTask_RecordsPdfDefaultAppChanges) {
  base::UserActionTester user_action_tester;

  // Non-PDF file types are not recorded.
  TaskDescriptor other_app_task("other-app-id", TASK_TYPE_FILE_HANDLER,
                                "action-id");
  UpdateDefaultTask(profile(), other_app_task, {".txt"}, {"text/plain"});
  // Even if it's the Media App.
  TaskDescriptor media_app_task(web_app::kMediaAppId, TASK_TYPE_FILE_HANDLER,
                                "action-id");
  UpdateDefaultTask(profile(), media_app_task, {"tiff"}, {"image/tiff"});

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedAway"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedTo"));

  // PDF files are recorded.
  UpdateDefaultTask(profile(), media_app_task, {".pdf"}, {"application/pdf"});

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedTo"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedAway"));
  user_action_tester.ResetCounts();

  UpdateDefaultTask(profile(), other_app_task, {".pdf"}, {"application/pdf"});

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedTo"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedAway"));
}

TEST_F(FileManagerFileTaskPreferencesTest,
       ChooseAndSetDefault_MatchesWithAlternateAppServiceTaskDescriptorForm) {
  base::test::ScopedFeatureList scoped_feature_list{
      ash::features::kArcFileTasksUseAppService};

  std::string package = "com.example.gallery";
  std::string activity = "com.example.gallery.OpenActivity";
  std::string app_id = "zabcdefg";
  TaskType task_type = TASK_TYPE_ARC_APP;

  AddFakeAppToAppService(app_id, package, /*policy_ids=*/{},
                         apps::AppType::kArc);

  // Set the default app preference.
  std::string files_app_id = package + "/" + activity;
  TaskDescriptor file_task(files_app_id, task_type, "view");
  base::Value::Dict mime_types;
  mime_types.Set("image/png", base::Value(TaskDescriptorToId(file_task)));
  UpdateDefaultTaskPreferences(mime_types, {});

  // Create the file task descriptors to match against.
  TaskDescriptor app_service_file_task(app_id, task_type, activity);
  TaskDescriptor other_task("other", TASK_TYPE_FILE_BROWSER_HANDLER, "view");

  auto resulting_tasks = std::make_unique<ResultingTasks>();
  std::vector<FullTaskDescriptor>& tasks = resulting_tasks->tasks;

  tasks.emplace_back(
      app_service_file_task, "View Images",
      GURL("http://example.com/some_icon.png"), false /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);
  tasks.emplace_back(other_task, "Other", GURL("http://example.com/other.text"),
                     false /* is_default */,
                     false /* is_generic_file_handler */,
                     false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"), "image/png",
                       false);

  // Check if the correct task matched against the default preference.
  ChooseAndSetDefaultTask(profile(), entries, resulting_tasks.get());
  ASSERT_TRUE(tasks[0].is_default);
  ASSERT_FALSE(tasks[1].is_default);
}

TEST_F(FileManagerFileTaskPreferencesTest,
       UpdateDefaultTask_ConvertsArcAppServiceTaskDescriptorToStandardTaskId) {
  base::test::ScopedFeatureList scoped_feature_list{
      ash::features::kArcFileTasksUseAppService};

  std::string package = "com.example.gallery";
  std::string activity = "com.example.gallery.OpenActivity";
  std::string app_id = "zabcdefg";
  TaskType task_type = TASK_TYPE_ARC_APP;
  std::string mime_type = "image/png";

  AddFakeAppToAppService(app_id, package, /*policy_ids=*/{},
                         apps::AppType::kArc);

  // Update default task preferences with our task descriptor (which is in the
  // format given from App Service file tasks).
  TaskDescriptor app_service_file_task(app_id, task_type, activity);
  UpdateDefaultTask(profile(), app_service_file_task, {}, {mime_type});

  // Check that the saved default preference is in the right format.
  std::string files_app_id = package + "/" + activity;
  std::string files_task_id = files_app_id + "|arc|view";
  const std::string* default_task_id =
      tasks_by_mime_type().FindString(mime_type);
  ASSERT_EQ(*default_task_id, files_task_id);
}

TEST_F(FileManagerFileTaskPreferencesTest,
       UpdateDefaultTask_SetsOfficeFileHandlersForGroup) {
  std::string app_id = "abcdef";
  TaskType task_type = TASK_TYPE_FILE_HANDLER;
  std::string activity = "first_activity";
  TaskDescriptor fake_office_task(app_id, task_type, activity);

  UpdateDefaultTask(profile(), fake_office_task, {".doc"},
                    {"application/msword"});

  std::string expected_task_id = MakeTaskID(app_id, task_type, activity);
  ASSERT_EQ(*tasks_by_mime_type().FindString("application/msword"),
            expected_task_id);
  ASSERT_EQ(*tasks_by_mime_type().FindString(
                "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"),
            expected_task_id);

  ASSERT_EQ(*tasks_by_suffix().FindString(".doc"), expected_task_id);
  ASSERT_EQ(*tasks_by_suffix().FindString(".docx"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".xls"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);

  ClearPrefs();

  UpdateDefaultTask(profile(), fake_office_task, {".xlsx"},
                    {"application/"
                     "vnd.openxmlformats-officedocument.spreadsheetml.sheet"});

  ASSERT_EQ(*tasks_by_mime_type().FindString("application/vnd.ms-excel"),
            expected_task_id);
  ASSERT_EQ(*tasks_by_mime_type().FindString(
                "application/vnd.ms-excel.sheet.macroEnabled.12"),
            expected_task_id);
  ASSERT_EQ(*tasks_by_mime_type().FindString(
                "application/"
                "vnd.openxmlformats-officedocument.spreadsheetml.sheet"),
            expected_task_id);

  ASSERT_EQ(*tasks_by_suffix().FindString(".xls"), expected_task_id);
  ASSERT_EQ(*tasks_by_suffix().FindString(".xlsm"), expected_task_id);
  ASSERT_EQ(*tasks_by_suffix().FindString(".xlsx"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".doc"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);

  ClearPrefs();

  UpdateDefaultTask(
      profile(), fake_office_task, {".pptx"},
      {"application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation"});

  ASSERT_EQ(*tasks_by_mime_type().FindString("application/vnd.ms-powerpoint"),
            expected_task_id);
  ASSERT_EQ(
      *tasks_by_mime_type().FindString(
          "application/"
          "vnd.openxmlformats-officedocument.presentationml.presentation"),
      expected_task_id);

  ASSERT_EQ(*tasks_by_suffix().FindString(".ppt"), expected_task_id);
  ASSERT_EQ(*tasks_by_suffix().FindString(".pptx"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".doc"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".xlsm"), nullptr);
}

TEST_F(FileManagerFileTaskPreferencesTest,
       UpdateDefaultTask_DoesNotSetOfficeFileHandlersForGroup) {
  std::string app_id = "abcdef";
  TaskType task_type = TASK_TYPE_FILE_HANDLER;
  std::string activity = "first_activity";
  TaskDescriptor fake_office_task(app_id, task_type, activity);

  UpdateDefaultTask(profile(), fake_office_task, {".doc"}, {});

  std::string expected_task_id = MakeTaskID(app_id, task_type, activity);
  ASSERT_EQ(tasks_by_mime_type().FindString("application/msword"), nullptr);
  ASSERT_EQ(tasks_by_mime_type().FindString(
                "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"),
            nullptr);

  ASSERT_EQ(*tasks_by_suffix().FindString(".doc"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".docx"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".xls"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);

  ClearPrefs();

  UpdateDefaultTask(profile(), fake_office_task, {}, {"application/msword"});

  ASSERT_EQ(*tasks_by_mime_type().FindString("application/msword"),
            expected_task_id);
  ASSERT_EQ(tasks_by_mime_type().FindString(
                "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"),
            nullptr);

  ASSERT_EQ(tasks_by_suffix().FindString(".doc"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".docx"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".xls"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);

  ClearPrefs();

  UpdateDefaultTask(profile(), fake_office_task, {".doc", ".xls"},
                    {"application/msword", "application/vnd.ms-excel"});

  ASSERT_EQ(*tasks_by_mime_type().FindString("application/msword"),
            expected_task_id);
  ASSERT_EQ(*tasks_by_mime_type().FindString("application/vnd.ms-excel"),
            expected_task_id);
  ASSERT_EQ(tasks_by_mime_type().FindString(
                "application/vnd.ms-excel.sheet.macroEnabled.12"),
            nullptr);

  ASSERT_EQ(*tasks_by_suffix().FindString(".doc"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".docx"), nullptr);
  ASSERT_EQ(*tasks_by_suffix().FindString(".xls"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".xlsm"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".xlsx"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);
}

// Test the setting of a default file task for Office files to a Files App SWA.
TEST_F(FileManagerFileTaskPreferencesTest, SetOfficeFileHandlersToFilesSWA) {
  file_manager::file_tasks::TaskDescriptor default_task;
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      "chrome://file-manager/?a");

  // Check no default tasks exist for Doc files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), "application/msword", ".doc", &default_task));
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(),
      "application/"
      "vnd.openxmlformats-officedocument.wordprocessingml.document",
      ".docx", &default_task));
  // Set default task for Doc files as a Files App SWA with action id "a".
  SetWordFileHandlerToFilesSWA(profile(), "a");
  // Check the default task for Doc files is `task`.
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), "application/msword", ".doc", &default_task));
  ASSERT_EQ(task, default_task);
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(),
      "application/"
      "vnd.openxmlformats-officedocument.wordprocessingml.document",
      ".docx", &default_task));
  ASSERT_EQ(task, default_task);

  // Check no default tasks exist for Excel files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), "application/vnd.ms-excel", ".xls",
      &default_task));
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(),
      "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
      ".xlsx", &default_task));
  // Set default task for Excel files as a Files App SWA with action id "a".
  SetExcelFileHandlerToFilesSWA(profile(), "a");
  // Check the default task for Excel files is `task`.
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), "application/vnd.ms-excel", ".xls",
      &default_task));
  ASSERT_EQ(task, default_task);
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(),
      "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
      ".xlsx", &default_task));
  ASSERT_EQ(task, default_task);

  // Check no default tasks exist for Powerpoint files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), "application/vnd.ms-powerpoint", ".ppt",
      &default_task));
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(),
      "application/"
      "vnd.openxmlformats-officedocument.presentationml.presentation",
      ".pptx", &default_task));
  // Set default task for Powerpoint files as a Files App SWA with action id
  // "a".
  SetPowerPointFileHandlerToFilesSWA(profile(), "a");
  // Check the default task for Powerpoint files is `task`.
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), "application/vnd.ms-powerpoint", ".ppt",
      &default_task));
  ASSERT_EQ(task, default_task);
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(),
      "application/"
      "vnd.openxmlformats-officedocument.presentationml.presentation",
      ".pptx", &default_task));
  ASSERT_EQ(task, default_task);
}

// Test using the test extension system, which needs lots of setup.
class FileManagerFileTasksComplexTest : public testing::Test {
 protected:
  FileManagerFileTasksComplexTest()
      : test_profile_(std::make_unique<TestingProfile>()),
        command_line_(base::CommandLine::NO_PROGRAM),
        extension_service_(nullptr) {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(test_profile_.get()));
    extension_service_ = test_extension_system->CreateExtensionService(
        &command_line_, base::FilePath() /* install_directory */,
        false /* autoupdate_enabled*/);
  }

  // Helper class for calling FindAllTypesOfTask synchronously.
  class FindAllTypesOfTasksSynchronousWrapper {
   public:
    void Call(Profile* profile,
              const std::vector<extensions::EntryInfo>& entries,
              const std::vector<GURL>& file_urls,
              const std::vector<std::string>& source_urls,
              ResultingTasks* resulting_tasks) {
      FindAllTypesOfTasks(
          profile, entries, file_urls, source_urls,
          base::BindOnce(&FindAllTypesOfTasksSynchronousWrapper::OnReply,
                         base::Unretained(this), resulting_tasks));
      run_loop_.Run();
    }

   private:
    void OnReply(ResultingTasks* out, std::unique_ptr<ResultingTasks> result) {
      *out = *result;
      run_loop_.Quit();
    }

    base::RunLoop run_loop_;
  };

  content::BrowserTaskEnvironment task_environment_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  ash::ScopedTestUserManager test_user_manager_;
  std::unique_ptr<TestingProfile> test_profile_;
  base::CommandLine command_line_;
  raw_ptr<extensions::ExtensionService, ExperimentalAsh>
      extension_service_;  // Owned by test_profile_;
};

}  // namespace file_manager::file_tasks
