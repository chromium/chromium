// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
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
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using extensions::api::file_manager_private::Verb;

namespace file_manager {
namespace file_tasks {
namespace {

// Registers the default task preferences. Used for testing
// ChooseAndSetDefaultTask().
void RegisterDefaultTaskPreferences(TestingPrefServiceSimple* pref_service) {
  DCHECK(pref_service);

  pref_service->registry()->RegisterDictionaryPref(
      prefs::kDefaultTasksByMimeType);
  pref_service->registry()->RegisterDictionaryPref(
      prefs::kDefaultTasksBySuffix);
}

// Updates the default task preferences per the given dictionary values. Used
// for testing ChooseAndSetDefaultTask.
void UpdateDefaultTaskPreferences(TestingPrefServiceSimple* pref_service,
                                  const base::DictionaryValue& mime_types,
                                  const base::DictionaryValue& suffixes) {
  DCHECK(pref_service);

  pref_service->Set(prefs::kDefaultTasksByMimeType, mime_types);
  pref_service->Set(prefs::kDefaultTasksBySuffix, suffixes);
}

}  // namespace

TEST(FileManagerFileTasksTest, FullTaskDescriptor_WithIconAndDefault) {
  FullTaskDescriptor full_descriptor(
      TaskDescriptor("app-id", TASK_TYPE_FILE_BROWSER_HANDLER, "action-id"),
      "task title", Verb::VERB_OPEN_WITH, GURL("http://example.com/icon.png"),
      true /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);

  EXPECT_EQ("app-id", full_descriptor.task_descriptor.app_id);
  EXPECT_EQ(TaskType::TASK_TYPE_FILE_BROWSER_HANDLER,
            full_descriptor.task_descriptor.task_type);
  EXPECT_EQ("action-id", full_descriptor.task_descriptor.action_id);
  EXPECT_EQ("http://example.com/icon.png", full_descriptor.icon_url.spec());
  EXPECT_EQ("task title", full_descriptor.task_title);
  EXPECT_EQ(Verb::VERB_OPEN_WITH, full_descriptor.task_verb);
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
            TaskDescriptorToId(TaskDescriptor("app-id",
                                              TASK_TYPE_FILE_BROWSER_HANDLER,
                                              "action-id")));
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

// Test that the right task is chosen from multiple choices per mime types
// and file extensions.
TEST(FileManagerFileTasksTest, ChooseAndSetDefaultTask_MultipleTasks) {
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);

  // Text.app and Nice.app were found for "foo.txt".
  TaskDescriptor text_app_task("text-app-id",
                               TASK_TYPE_FILE_HANDLER,
                               "action-id");
  TaskDescriptor nice_app_task("nice-app-id",
                               TASK_TYPE_FILE_HANDLER,
                               "action-id");
  std::vector<FullTaskDescriptor> tasks;
  tasks.emplace_back(
      text_app_task, "Text.app", Verb::VERB_OPEN_WITH,
      GURL("http://example.com/text_app.png"), false /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);
  tasks.emplace_back(
      nice_app_task, "Nice.app", Verb::VERB_ADD_TO,
      GURL("http://example.com/nice_app.png"), false /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"), "text/plain",
                       false);

  // None of them should be chosen as default, as nothing is set in the
  // preferences.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_FALSE(tasks[0].is_default);
  EXPECT_FALSE(tasks[1].is_default);

  // Set Text.app as default for "text/plain" in the preferences.
  base::DictionaryValue empty;
  base::DictionaryValue mime_types;
  mime_types.SetKey("text/plain",
                    base::Value(TaskDescriptorToId(text_app_task)));
  UpdateDefaultTaskPreferences(&pref_service, mime_types, empty);

  // Text.app should be chosen as default.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_TRUE(tasks[0].is_default);
  EXPECT_FALSE(tasks[1].is_default);

  // Change it back to non-default for testing further.
  tasks[0].is_default = false;

  // Clear the preferences and make sure none of them are default.
  UpdateDefaultTaskPreferences(&pref_service, empty, empty);
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_FALSE(tasks[0].is_default);
  EXPECT_FALSE(tasks[1].is_default);

  // Set Nice.app as default for ".txt" in the preferences.
  base::DictionaryValue suffixes;
  suffixes.SetKey(".txt", base::Value(TaskDescriptorToId(nice_app_task)));
  UpdateDefaultTaskPreferences(&pref_service, empty, suffixes);

  // Now Nice.app should be chosen as default.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_FALSE(tasks[0].is_default);
  EXPECT_TRUE(tasks[1].is_default);
}

// Test that internal file browser handler of the Files app is chosen as
// default even if nothing is set in the preferences.
TEST(FileManagerFileTasksTest, ChooseAndSetDefaultTask_FallbackFileBrowser) {
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);

  // The internal file browser handler of the Files app was found for "foo.txt".
  TaskDescriptor files_app_task(kFileManagerAppId,
                                TASK_TYPE_FILE_BROWSER_HANDLER,
                                "view-in-browser");
  std::vector<FullTaskDescriptor> tasks;
  tasks.emplace_back(
      files_app_task, "View in browser", Verb::VERB_OPEN_WITH,
      GURL("http://example.com/some_icon.png"), false /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"), "text/plain",
                       false);

  // The internal file browser handler should be chosen as default, as it's a
  // fallback file browser handler.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_TRUE(tasks[0].is_default);
}

// Test that Text.app is chosen as default instead of the Files app
// even if nothing is set in the preferences.
TEST(FileManagerFileTasksTest, ChooseAndSetDefaultTask_FallbackTextApp) {
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);

  // Define the browser handler of the Files app for "foo.txt".
  TaskDescriptor files_app_task(
      kFileManagerAppId, TASK_TYPE_FILE_BROWSER_HANDLER, "view-in-browser");
  // Define the text editor app for "foo.txt".
  TaskDescriptor text_app_task(kTextEditorAppId, TASK_TYPE_FILE_HANDLER,
                               "Text");
  std::vector<FullTaskDescriptor> tasks;
  tasks.emplace_back(
      files_app_task, "View in browser", Verb::VERB_OPEN_WITH,
      GURL("http://example.com/some_icon.png"), false /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);
  tasks.emplace_back(
      text_app_task, "Text", Verb::VERB_OPEN_WITH,
      GURL("chrome://extension-icon/mmfbcljfglbokpmkimbfghdkjmjhdgbg/16/1"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"), "text/plain",
                       false);

  // The text editor app should be chosen as default, as it's a fallback file
  // browser handler.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_TRUE(tasks[1].is_default);
}

// Test that browser is chosen as default for HTML files instead of the Text
// app even if nothing is set in the preferences.
TEST(FileManagerFileTasksTest, ChooseAndSetDefaultTask_FallbackHtmlTextApp) {
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);

  // Define the browser handler of the Files app for "foo.html".
  TaskDescriptor files_app_task(
      kFileManagerAppId, TASK_TYPE_FILE_BROWSER_HANDLER, "view-in-browser");
  // Define the text editor app for "foo.html".
  TaskDescriptor text_app_task(kTextEditorAppId, TASK_TYPE_FILE_HANDLER,
                               "Text");
  std::vector<FullTaskDescriptor> tasks;
  tasks.emplace_back(
      files_app_task, "View in browser", Verb::VERB_OPEN_WITH,
      GURL("http://example.com/some_icon.png"), false /* is_default */,
      false /* is_generic_file_handler */, false /* is_file_extension_match */);
  tasks.emplace_back(
      text_app_task, "Text", Verb::VERB_OPEN_WITH,
      GURL("chrome://extension-icon/mmfbcljfglbokpmkimbfghdkjmjhdgbg/16/1"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.html"), "text/html",
                       false);

  // The internal file browser handler should be chosen as default,
  // as it's a fallback file browser handler.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_TRUE(tasks[0].is_default);
}

// Test that Office Editing is chosen as default even if nothing is set in the
// preferences.
TEST(FileManagerFileTasksTest, ChooseAndSetDefaultTask_FallbackOfficeEditing) {
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);

  // The Office Editing app was found for "slides.pptx".
  TaskDescriptor files_app_task(
      extension_misc::kQuickOfficeComponentExtensionId, TASK_TYPE_FILE_HANDLER,
      "Office Editing for Docs, Sheets & Slides");
  std::vector<FullTaskDescriptor> tasks;
  tasks.emplace_back(
      files_app_task, "Office Editing for Docs, Sheets & Slides",
      Verb::VERB_OPEN_WITH,
      GURL("chrome://extension-icon/bpmcpldpdmajfigpchkicefoigmkfalc/32/1"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("slides.pptx"), "",
                       false);

  // The Office Editing app should be chosen as default, as it's a fallback
  // file browser handler.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_TRUE(tasks[0].is_default);
}

// Test that for changes of default app for PDF files, a metric is recorded.
TEST(FileManagerFileTasksTest, UpdateDefaultTask_RecordsPdfDefaultAppChanges) {
  base::test::ScopedFeatureList scoped_feature_list{
      ash::features::kMediaAppHandlesPdf};
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);
  base::UserActionTester user_action_tester;

  // Non-PDF file types are not recorded.
  TaskDescriptor other_app_task("other-app-id", TASK_TYPE_FILE_HANDLER,
                                "action-id");
  UpdateDefaultTask(&pref_service, other_app_task, {".txt"}, {"text/plain"});
  // Even if it's the Media App.
  TaskDescriptor media_app_task(web_app::kMediaAppId, TASK_TYPE_FILE_HANDLER,
                                "action-id");
  UpdateDefaultTask(&pref_service, media_app_task, {"tiff"}, {"image/tiff"});

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedAway"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedTo"));

  // PDF files are recorded.
  UpdateDefaultTask(&pref_service, media_app_task, {".pdf"},
                    {"application/pdf"});

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedTo"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedAway"));
  user_action_tester.ResetCounts();

  UpdateDefaultTask(&pref_service, other_app_task, {".pdf"},
                    {"application/pdf"});

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedTo"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MediaApp.PDF.DefaultApp.SwitchedAway"));
}

// Test FileHandlerIsEnabled which returns whether a file handler should be
// used.
TEST(FileManagerFileTasksTest, FileHandlerIsEnabled) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile test_profile;
  crostini::FakeCrostiniFeatures crostini_features;

  const std::string test_id = "test";

  crostini_features.set_export_import_ui_allowed(true);
  EXPECT_TRUE(FileHandlerIsEnabled(&test_profile, "import-crostini-image"));
  EXPECT_TRUE(FileHandlerIsEnabled(&test_profile, test_id));

  crostini_features.set_export_import_ui_allowed(false);
  EXPECT_FALSE(FileHandlerIsEnabled(&test_profile, "import-crostini-image"));
  EXPECT_TRUE(FileHandlerIsEnabled(&test_profile, test_id));

  crostini_features.set_root_access_allowed(true);
  EXPECT_TRUE(FileHandlerIsEnabled(&test_profile, "install-linux-package"));
  EXPECT_TRUE(FileHandlerIsEnabled(&test_profile, test_id));

  crostini_features.set_root_access_allowed(false);
  EXPECT_FALSE(FileHandlerIsEnabled(&test_profile, "install-linux-package"));
  EXPECT_TRUE(FileHandlerIsEnabled(&test_profile, test_id));
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
        &command_line_,
        base::FilePath()  /* install_directory */,
        false  /* autoupdate_enabled*/);
  }

  // Helper class for calling FindAllTypesOfTask synchronously.
  class FindAllTypesOfTasksSynchronousWrapper {
   public:
    void Call(Profile* profile,
              const std::vector<extensions::EntryInfo>& entries,
              const std::vector<GURL>& file_urls,
              std::vector<FullTaskDescriptor>* result) {
      FindAllTypesOfTasks(
          profile, entries, file_urls,
          base::BindOnce(&FindAllTypesOfTasksSynchronousWrapper::OnReply,
                         base::Unretained(this), result));
      run_loop_.Run();
    }

   private:
    void OnReply(std::vector<FullTaskDescriptor>* out,
                 std::unique_ptr<std::vector<FullTaskDescriptor>> result) {
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
  extensions::ExtensionService* extension_service_;  // Owned by test_profile_;
};

// The basic logic is similar to a test case for FindFileHandlerTasks above.
TEST_F(FileManagerFileTasksComplexTest, FindFileBrowserHandlerTasks) {
  // Copied from FindFileHandlerTasks test above.
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";
  const char kBarId[] = "odlhccgofgkadkkhcmhgnhgahonahoca";

  // Foo.app can handle ".txt" and ".html".
  // This one is an extension, and has "file_browser_handlers"
  extensions::ExtensionBuilder foo_app;
  foo_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("permissions",
               extensions::ListBuilder().Append("fileBrowserHandler").Build())
          .Set("file_browser_handlers",
               extensions::ListBuilder()
                   .Append(
                       extensions::DictionaryBuilder()
                           .Set("id", "open")
                           .Set("default_title", "open")
                           .Set("file_filters", extensions::ListBuilder()
                                                    .Append("filesystem:*.txt")
                                                    .Append("filesystem:*.html")
                                                    .Build())
                           .Build())
                   .Build())
          .Build());
  foo_app.SetID(kFooId);
  extension_service_->AddExtension(foo_app.Build().get());

  // Bar.app can only handle ".txt".
  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Bar")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("permissions",
               extensions::ListBuilder().Append("fileBrowserHandler").Build())
          .Set("file_browser_handlers",
               extensions::ListBuilder()
                   .Append(
                       extensions::DictionaryBuilder()
                           .Set("id", "open")
                           .Set("default_title", "open")
                           .Set("file_filters", extensions::ListBuilder()
                                                    .Append("filesystem:*.txt")
                                                    .Build())
                           .Build())
                   .Build())
          .Build());
  bar_app.SetID(kBarId);
  extension_service_->AddExtension(bar_app.Build().get());

  // Find apps for a ".txt" file. Foo.app and Bar.app should be found.
  std::vector<GURL> file_urls;
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.txt");

  std::vector<FullTaskDescriptor> tasks;
  FindFileBrowserHandlerTasks(test_profile_.get(), file_urls, &tasks);
  ASSERT_EQ(2U, tasks.size());
  // Sort the app IDs, as the order is not guaranteed.
  std::vector<std::string> app_ids;
  app_ids.push_back(tasks[0].task_descriptor.app_id);
  app_ids.push_back(tasks[1].task_descriptor.app_id);
  std::sort(app_ids.begin(), app_ids.end());
  // Confirm that both Foo.app and Bar.app are found.
  EXPECT_EQ(kFooId, app_ids[0]);
  EXPECT_EQ(kBarId, app_ids[1]);

  // Find apps for ".txt" and ".html" files. Only Foo.app should be found.
  file_urls.clear();
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.txt");
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.html");
  tasks.clear();
  FindFileBrowserHandlerTasks(test_profile_.get(), file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  // Confirm that only Foo.app is found.
  EXPECT_EQ(kFooId, tasks[0].task_descriptor.app_id);

  // Add an ".png" file. No tasks should be found.
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.png");
  tasks.clear();
  FindFileBrowserHandlerTasks(test_profile_.get(), file_urls, &tasks);
  // Confirm no tasks are found.
  ASSERT_TRUE(tasks.empty());
}

// Test using the test extension system, which needs lots of setup.
class FileManagerFileTasksCrostiniTest
    : public FileManagerFileTasksComplexTest {
 protected:
  FileManagerFileTasksCrostiniTest()
      : crostini_test_helper_(std::make_unique<crostini::CrostiniTestHelper>(
            test_profile_.get())),
        crostini_folder_(util::GetCrostiniMountDirectory(test_profile_.get())) {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    vm_tools::apps::App text_app =
        crostini::CrostiniTestHelper::BasicApp("text_app");
    *text_app.add_mime_types() = "text/plain";
    crostini_test_helper_->AddApp(text_app);

    vm_tools::apps::App image_app =
        crostini::CrostiniTestHelper::BasicApp("image_app");
    *image_app.add_mime_types() = "image/gif";
    *image_app.add_mime_types() = "image/jpeg";
    *image_app.add_mime_types() = "image/jpg";
    *image_app.add_mime_types() = "image/png";
    crostini_test_helper_->AddApp(image_app);

    vm_tools::apps::App gif_app =
        crostini::CrostiniTestHelper::BasicApp("gif_app");
    *gif_app.add_mime_types() = "image/gif";
    crostini_test_helper_->AddApp(gif_app);

    vm_tools::apps::App alt_mime_app =
        crostini::CrostiniTestHelper::BasicApp("alt_mime_app");
    *alt_mime_app.add_mime_types() = "foo/x-bar";
    crostini_test_helper_->AddApp(alt_mime_app);

    text_app_id_ = crostini::CrostiniTestHelper::GenerateAppId("text_app");
    image_app_id_ = crostini::CrostiniTestHelper::GenerateAppId("image_app");
    gif_app_id_ = crostini::CrostiniTestHelper::GenerateAppId("gif_app");
    alt_mime_app_id_ =
        crostini::CrostiniTestHelper::GenerateAppId("alt_mime_app");

    // Setup the custom MIME type mapping.
    vm_tools::apps::MimeTypes mime_types_list;
    mime_types_list.set_vm_name(crostini::kCrostiniDefaultVmName);
    mime_types_list.set_container_name(crostini::kCrostiniDefaultContainerName);
    (*mime_types_list.mutable_mime_type_mappings())["foo"] = "foo/x-bar";

    guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(test_profile_.get())
        ->UpdateMimeTypes(mime_types_list);
  }
  ~FileManagerFileTasksCrostiniTest() override {
    crostini_test_helper_.reset();
    test_profile_.reset();
    ash::ConciergeClient::Shutdown();
  }

  void SetUp() override {
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        util::GetDownloadsMountPointName(test_profile_.get()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        util::GetMyFilesFolderForProfile(test_profile_.get()));
  }

  void TearDown() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        util::GetDownloadsMountPointName(test_profile_.get()));
  }

  GURL PathToURL(const std::string& path) {
    std::string virtual_path = base::EscapeUrlEncodedData(
        util::GetDownloadsMountPointName(test_profile_.get()) + "/" + path,
        /*use_plus=*/false);
    return GURL("filesystem:chrome-extension://id/external/" + virtual_path);
  }

  std::unique_ptr<crostini::CrostiniTestHelper> crostini_test_helper_;
  base::FilePath crostini_folder_;
  std::string text_app_id_;
  std::string image_app_id_;
  std::string gif_app_id_;
  std::string alt_mime_app_id_;
};

TEST_F(FileManagerFileTasksCrostiniTest, BasicFiles) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("foo.txt"), "text/plain", false}};
  std::vector<GURL> file_urls{PathToURL("dir/foo.txt")};

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(test_profile_.get(), entries,
                                               file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(text_app_id_, tasks[0].task_descriptor.app_id);

  // Multiple text files
  entries.emplace_back(crostini_folder_.Append("bar.txt"), "text/plain", false);
  file_urls.emplace_back(PathToURL("dir/bar.txt"));
  FindAllTypesOfTasksSynchronousWrapper().Call(test_profile_.get(), entries,
                                               file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(text_app_id_, tasks[0].task_descriptor.app_id);
}

TEST_F(FileManagerFileTasksCrostiniTest, Directories) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("dir"), "", true}};
  std::vector<GURL> file_urls{PathToURL("dir/dir")};
  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(test_profile_.get(), entries,
                                               file_urls, &tasks);
  EXPECT_EQ(0U, tasks.size());

  entries.emplace_back(crostini_folder_.Append("foo.txt"), "text/plain", false);
  file_urls.emplace_back(PathToURL("dir/foo.txt"));
  FindAllTypesOfTasksSynchronousWrapper().Call(test_profile_.get(), entries,
                                               file_urls, &tasks);
  EXPECT_EQ(0U, tasks.size());
}

TEST_F(FileManagerFileTasksCrostiniTest, MultipleMatches) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("foo.gif"), "image/gif", false},
      {crostini_folder_.Append("bar.gif"), "image/gif", false}};
  std::vector<GURL> file_urls{PathToURL("dir/foo.gif"),
                              PathToURL("dir/bar.gif")};

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(test_profile_.get(), entries,
                                               file_urls, &tasks);
  // The returned values happen to be ordered alphabetically by app_id, so we
  // rely on this to keep the test simple.
  EXPECT_LT(gif_app_id_, image_app_id_);
  ASSERT_EQ(2U, tasks.size());
  EXPECT_EQ(gif_app_id_, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(image_app_id_, tasks[1].task_descriptor.app_id);
}

TEST_F(FileManagerFileTasksCrostiniTest, MultipleTypes) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("foo.gif"), "image/gif", false},
      {crostini_folder_.Append("bar.png"), "image/png", false}};
  std::vector<GURL> file_urls{PathToURL("dir/foo.gif"),
                              PathToURL("dir/bar.png")};

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(test_profile_.get(), entries,
                                               file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(image_app_id_, tasks[0].task_descriptor.app_id);

  entries.emplace_back(crostini_folder_.Append("qux.mp4"), "video/mp4", false);
  file_urls.emplace_back(PathToURL("dir/qux.mp4"));
  FindAllTypesOfTasksSynchronousWrapper().Call(test_profile_.get(), entries,
                                               file_urls, &tasks);
  EXPECT_EQ(0U, tasks.size());
}

TEST_F(FileManagerFileTasksCrostiniTest, AlternateMimeTypes) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("bar1.foo"), "text/plain", false},
      {crostini_folder_.Append("bar2.foo"), "application/octet-stream", false}};
  std::vector<GURL> file_urls{PathToURL("dir/bar1.foo"),
                              PathToURL("dir/bar2.foo")};

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(test_profile_.get(), entries,
                                               file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(alt_mime_app_id_, tasks[0].task_descriptor.app_id);
}

}  // namespace file_tasks
}  // namespace file_manager.
