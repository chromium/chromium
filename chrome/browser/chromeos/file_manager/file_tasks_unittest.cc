// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
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

  const std::string task_id =
      TaskDescriptorToId(full_descriptor.task_descriptor());
  EXPECT_EQ("app-id|file|action-id", task_id);
  EXPECT_EQ("http://example.com/icon.png", full_descriptor.icon_url().spec());
  EXPECT_EQ("task title", full_descriptor.task_title());
  EXPECT_EQ(Verb::VERB_OPEN_WITH, full_descriptor.task_verb());
  EXPECT_TRUE(full_descriptor.is_default());
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
  EXPECT_FALSE(tasks[0].is_default());
  EXPECT_FALSE(tasks[1].is_default());

  // Set Text.app as default for "text/plain" in the preferences.
  base::DictionaryValue empty;
  base::DictionaryValue mime_types;
  mime_types.SetKey("text/plain",
                    base::Value(TaskDescriptorToId(text_app_task)));
  UpdateDefaultTaskPreferences(&pref_service, mime_types, empty);

  // Text.app should be chosen as default.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_TRUE(tasks[0].is_default());
  EXPECT_FALSE(tasks[1].is_default());

  // Change it back to non-default for testing further.
  tasks[0].set_is_default(false);

  // Clear the preferences and make sure none of them are default.
  UpdateDefaultTaskPreferences(&pref_service, empty, empty);
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_FALSE(tasks[0].is_default());
  EXPECT_FALSE(tasks[1].is_default());

  // Set Nice.app as default for ".txt" in the preferences.
  base::DictionaryValue suffixes;
  suffixes.SetKey(".txt", base::Value(TaskDescriptorToId(nice_app_task)));
  UpdateDefaultTaskPreferences(&pref_service, empty, suffixes);

  // Now Nice.app should be chosen as default.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_FALSE(tasks[0].is_default());
  EXPECT_TRUE(tasks[1].is_default());
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
  EXPECT_TRUE(tasks[0].is_default());
}

// Test that Text.app is chosen as default even if nothing is set in the
// preferences.
TEST(FileManagerFileTasksTest, ChooseAndSetDefaultTask_FallbackTextApp) {
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);

  // The text editor app was found for "foo.txt".
  TaskDescriptor files_app_task(kTextEditorAppId, TASK_TYPE_FILE_HANDLER,
                                "Text");
  std::vector<FullTaskDescriptor> tasks;
  tasks.emplace_back(
      files_app_task, "Text", Verb::VERB_OPEN_WITH,
      GURL("chrome://extension-icon/mmfbcljfglbokpmkimbfghdkjmjhdgbg/16/1"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.txt"), "text/plain",
                       false);

  // The text editor app should be chosen as default, as it's a fallback file
  // browser handler.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_TRUE(tasks[0].is_default());
}

// Test that Audio Player is chosen as default even if nothing is set in the
// preferences.
TEST(FileManagerFileTasksTest, ChooseAndSetDefaultTask_FallbackAudioPlayer) {
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);

  // The Audio Player app was found for "sound.wav".
  TaskDescriptor files_app_task(kAudioPlayerAppId, TASK_TYPE_FILE_HANDLER,
                                "Audio Player");
  std::vector<FullTaskDescriptor> tasks;
  tasks.emplace_back(
      files_app_task, "Audio Player", Verb::VERB_OPEN_WITH,
      GURL("chrome://extension-icon/cjbfomnbifhcdnihkgipgfcihmgjfhbf/32/1"),
      false /* is_default */, false /* is_generic_file_handler */,
      false /* is_file_extension_match */);
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("sound.wav"), "audio/wav",
                       false);

  // The Audio Player app should be chosen as default, as it's a fallback file
  // browser handler.
  ChooseAndSetDefaultTask(pref_service, entries, &tasks);
  EXPECT_TRUE(tasks[0].is_default());
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
  EXPECT_TRUE(tasks[0].is_default());
}

// Test IsFileHandlerEnabled which returns whether a file handler should be
// used.
TEST(FileManagerFileTasksTest, IsFileHandlerEnabled) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile test_profile;
  crostini::FakeCrostiniFeatures crostini_features;

  apps::FileHandlerInfo test_handler;
  test_handler.id = "test";

  // Test import-crostini-image.
  apps::FileHandlerInfo crostini_import_handler;
  crostini_import_handler.id = "import-crostini-image";
  crostini_features.set_export_import_ui_allowed(true);
  EXPECT_TRUE(IsFileHandlerEnabled(&test_profile, crostini_import_handler));
  EXPECT_TRUE(IsFileHandlerEnabled(&test_profile, test_handler));

  crostini_features.set_export_import_ui_allowed(false);
  EXPECT_FALSE(IsFileHandlerEnabled(&test_profile, crostini_import_handler));
  EXPECT_TRUE(IsFileHandlerEnabled(&test_profile, test_handler));

  // Test install-linux-package.
  apps::FileHandlerInfo install_linux_handler;
  install_linux_handler.id = "install-linux-package";
  crostini_features.set_root_access_allowed(true);
  EXPECT_TRUE(IsFileHandlerEnabled(&test_profile, install_linux_handler));
  EXPECT_TRUE(IsFileHandlerEnabled(&test_profile, test_handler));

  crostini_features.set_root_access_allowed(false);
  EXPECT_FALSE(IsFileHandlerEnabled(&test_profile, install_linux_handler));
  EXPECT_TRUE(IsFileHandlerEnabled(&test_profile, test_handler));
}

// Test IsGoodMatchFileHandler which returns whether a file handle info matches
// with files as good match or not.
TEST(FileManagerFileTasksTest, IsGoodMatchFileHandler) {
  using FileHandlerInfo = apps::FileHandlerInfo;

  std::vector<extensions::EntryInfo> entries_1;
  entries_1.emplace_back(base::FilePath(FILE_PATH_LITERAL("foo.jpg")),
                         "image/jpeg", false);
  entries_1.emplace_back(base::FilePath(FILE_PATH_LITERAL("bar.txt")),
                         "text/plain", false);

  std::vector<extensions::EntryInfo> entries_2;
  entries_2.emplace_back(base::FilePath(FILE_PATH_LITERAL("foo.ics")),
                         "text/calendar", false);

  // extensions: ["*"]
  FileHandlerInfo file_handler_info_1;
  file_handler_info_1.extensions.insert("*");
  EXPECT_FALSE(IsGoodMatchFileHandler(file_handler_info_1, entries_1));

  // extensions: ["*", "jpg"]
  FileHandlerInfo file_handler_info_2;
  file_handler_info_2.extensions.insert("*");
  file_handler_info_2.extensions.insert("jpg");
  EXPECT_FALSE(IsGoodMatchFileHandler(file_handler_info_2, entries_1));

  // extensions: ["jpg"]
  FileHandlerInfo file_handler_info_3;
  file_handler_info_3.extensions.insert("jpg");
  EXPECT_TRUE(IsGoodMatchFileHandler(file_handler_info_3, entries_1));

  // types: ["*"]
  FileHandlerInfo file_handler_info_4;
  file_handler_info_4.types.insert("*");
  EXPECT_FALSE(IsGoodMatchFileHandler(file_handler_info_4, entries_1));

  // types: ["*/*"]
  FileHandlerInfo file_handler_info_5;
  file_handler_info_5.types.insert("*/*");
  EXPECT_FALSE(IsGoodMatchFileHandler(file_handler_info_5, entries_1));

  // types: ["image/*"]
  FileHandlerInfo file_handler_info_6;
  file_handler_info_6.types.insert("image/*");
  // Partial wild card is not generic.
  EXPECT_TRUE(IsGoodMatchFileHandler(file_handler_info_6, entries_1));

  // types: ["*", "image/*"]
  FileHandlerInfo file_handler_info_7;
  file_handler_info_7.types.insert("*");
  file_handler_info_7.types.insert("image/*");
  EXPECT_FALSE(IsGoodMatchFileHandler(file_handler_info_7, entries_1));

  // extensions: ["*"], types: ["image/*"]
  FileHandlerInfo file_handler_info_8;
  file_handler_info_8.extensions.insert("*");
  file_handler_info_8.types.insert("image/*");
  EXPECT_FALSE(IsGoodMatchFileHandler(file_handler_info_8, entries_1));

  // types: ["text/*"] and target files contain unsupported text mime type, e.g.
  // text/calendar.
  FileHandlerInfo file_handler_info_9;
  file_handler_info_9.types.insert("text/*");
  EXPECT_FALSE(IsGoodMatchFileHandler(file_handler_info_9, entries_2));

  // types: ["text/*"] and target files don't contain unsupported text mime
  // type.
  FileHandlerInfo file_handler_info_10;
  file_handler_info_10.types.insert("text/*");
  EXPECT_TRUE(IsGoodMatchFileHandler(file_handler_info_10, entries_1));

  // path_directory_set not empty.
  FileHandlerInfo file_handler_info_11;
  std::vector<extensions::EntryInfo> entries_3;
  entries_3.emplace_back(base::FilePath(FILE_PATH_LITERAL("dir1")), "", true);
  EXPECT_FALSE(IsGoodMatchFileHandler(file_handler_info_11, entries_3));
}

// Test using the test extension system, which needs lots of setup.
class FileManagerFileTasksComplexTest : public testing::Test {
 protected:
  FileManagerFileTasksComplexTest()
      : command_line_(base::CommandLine::NO_PROGRAM),
        extension_service_(nullptr) {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&test_profile_));
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
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager test_user_manager_;
  TestingProfile test_profile_;
  base::CommandLine command_line_;
  extensions::ExtensionService* extension_service_;  // Owned by test_profile_;
};

TEST_F(FileManagerFileTasksComplexTest, FindFileHandlerTasks) {
  // Random IDs generated by
  // % ruby -le 'print (0...32).to_a.map{(?a + rand(16)).chr}.join'
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";
  const char kBarId[] = "odlhccgofgkadkkhcmhgnhgahonahoca";

  // Foo.app can handle "text/plain" and "text/html".
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
          .Set("file_handlers",
               extensions::DictionaryBuilder()
                   .Set("text", extensions::DictionaryBuilder()
                                    .Set("title", "Text")
                                    .Set("types", extensions::ListBuilder()
                                                      .Append("text/plain")
                                                      .Append("text/html")
                                                      .Build())
                                    .Build())
                   .Build())
          .Build());
  foo_app.SetID(kFooId);
  extension_service_->AddExtension(foo_app.Build().get());

  // Bar.app can only handle "text/plain".
  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Bar")
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
          .Set("file_handlers",
               extensions::DictionaryBuilder()
                   .Set("text", extensions::DictionaryBuilder()
                                    .Set("title", "Text")
                                    .Set("types", extensions::ListBuilder()
                                                      .Append("text/plain")
                                                      .Build())
                                    .Build())
                   .Build())
          .Build());
  bar_app.SetID(kBarId);
  extension_service_->AddExtension(bar_app.Build().get());

  // Find apps for a "text/plain" file. Foo.app and Bar.app should be found.
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.txt"),
      "text/plain", false);

  std::vector<FullTaskDescriptor> tasks;
  FindFileHandlerTasks(&test_profile_, entries, &tasks);
  ASSERT_EQ(2U, tasks.size());
  // Sort the app IDs, as the order is not guaranteed.
  std::vector<std::string> app_ids;
  app_ids.push_back(tasks[0].task_descriptor().app_id);
  app_ids.push_back(tasks[1].task_descriptor().app_id);
  std::sort(app_ids.begin(), app_ids.end());
  // Confirm that both Foo.app and Bar.app are found.
  EXPECT_EQ(kFooId, app_ids[0]);
  EXPECT_EQ(kBarId, app_ids[1]);

  // Find apps for "text/plain" and "text/html" files. Only Foo.app should be
  // found.
  entries.clear();
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.txt"),
      "text/plain", false);
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.html"),
      "text/html", false);
  tasks.clear();
  FindFileHandlerTasks(&test_profile_, entries, &tasks);
  ASSERT_EQ(1U, tasks.size());
  // Confirm that only Foo.app is found.
  EXPECT_EQ(kFooId, tasks[0].task_descriptor().app_id);

  // Add an "image/png" file. No tasks should be found.
  entries.emplace_back(base::FilePath::FromUTF8Unsafe("foo.png"), "image/png",
                       false);
  tasks.clear();
  FindFileHandlerTasks(&test_profile_, entries, &tasks);
  // Confirm no tasks are found.
  ASSERT_TRUE(tasks.empty());
}

TEST_F(FileManagerFileTasksComplexTest,
       BookmarkAppsAreNotListedInFileHandlerTasks) {
  const char kGraphrId[] = "ppcpljkgngnngojbghcdiojhbneibgdg";
  const char kGraphrFileAction[] = "https://graphr.tld/open-files/?name=raw";
  extensions::ExtensionBuilder graphr;
  graphr.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Graphr")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app",
               extensions::DictionaryBuilder()
                   .Set("launch", extensions::DictionaryBuilder()
                                      .Set("web_url", "https://graphr.tld")
                                      .Build())
                   .Build())
          .Set(
              "file_handlers",
              extensions::DictionaryBuilder()
                  .Set(kGraphrFileAction,
                       extensions::DictionaryBuilder()
                           .Set("title", "Raw")
                           .Set("types", extensions::ListBuilder()
                                             .Append("text/csv")
                                             .Build())
                           .Set("extensions",
                                extensions::ListBuilder().Append("csv").Build())
                           .Build())
                  .Build())
          .Build());
  graphr.SetID(kGraphrId);
  graphr.AddFlags(extensions::Extension::InitFromValueFlags::FROM_BOOKMARK);

  extension_service_->AddExtension(graphr.Build().get());
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(&test_profile_);
  const extensions::Extension* extension = registry->GetExtensionById(
      kGraphrId, extensions::ExtensionRegistry::ENABLED);

  ASSERT_EQ(extension->GetType(), extensions::Manifest::Type::TYPE_HOSTED_APP);
  ASSERT_TRUE(extension->from_bookmark());

  std::vector<FullTaskDescriptor> tasks;
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.csv"),
      "text/csv", false);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({blink::features::kNativeFileSystemAPI,
                                        blink::features::kFileHandlingAPI},
                                       {});
  FindFileHandlerTasks(&test_profile_, entries, &tasks);
  EXPECT_EQ(0u, tasks.size());
}

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
  FindFileBrowserHandlerTasks(&test_profile_, file_urls, &tasks);
  ASSERT_EQ(2U, tasks.size());
  // Sort the app IDs, as the order is not guaranteed.
  std::vector<std::string> app_ids;
  app_ids.push_back(tasks[0].task_descriptor().app_id);
  app_ids.push_back(tasks[1].task_descriptor().app_id);
  std::sort(app_ids.begin(), app_ids.end());
  // Confirm that both Foo.app and Bar.app are found.
  EXPECT_EQ(kFooId, app_ids[0]);
  EXPECT_EQ(kBarId, app_ids[1]);

  // Find apps for ".txt" and ".html" files. Only Foo.app should be found.
  file_urls.clear();
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.txt");
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.html");
  tasks.clear();
  FindFileBrowserHandlerTasks(&test_profile_, file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  // Confirm that only Foo.app is found.
  EXPECT_EQ(kFooId, tasks[0].task_descriptor().app_id);

  // Add an ".png" file. No tasks should be found.
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.png");
  tasks.clear();
  FindFileBrowserHandlerTasks(&test_profile_, file_urls, &tasks);
  // Confirm no tasks are found.
  ASSERT_TRUE(tasks.empty());
}

// Test that all kinds of apps (file handler and file browser handler) are
// returned.
TEST_F(FileManagerFileTasksComplexTest, FindAllTypesOfTasks) {
  // kFooId and kBarId copied from FindFileHandlerTasks test above.
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";
  const char kBarId[] = "odlhccgofgkadkkhcmhgnhgahonahoca";

  // Foo.app can handle "text/plain".
  // This is a packaged app (file handler).
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
          .Set("file_handlers",
               extensions::DictionaryBuilder()
                   .Set("text", extensions::DictionaryBuilder()
                                    .Set("title", "Text")
                                    .Set("types", extensions::ListBuilder()
                                                      .Append("text/plain")
                                                      .Build())
                                    .Build())
                   .Build())
          .Build());
  foo_app.SetID(kFooId);
  extension_service_->AddExtension(foo_app.Build().get());

  // Bar.app can only handle ".txt".
  // This is an extension (file browser handler).
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

  // Find apps for "foo.txt". All apps should be found.
  std::vector<extensions::EntryInfo> entries;
  std::vector<GURL> file_urls;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.txt"),
      "text/plain", false);
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.txt");

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  ASSERT_EQ(2U, tasks.size());

  // Sort the app IDs, as the order is not guaranteed.
  std::vector<std::string> app_ids;
  app_ids.push_back(tasks[0].task_descriptor().app_id);
  app_ids.push_back(tasks[1].task_descriptor().app_id);
  std::sort(app_ids.begin(), app_ids.end());
  // Confirm that all apps are found.
  EXPECT_EQ(kFooId, app_ids[0]);
  EXPECT_EQ(kBarId, app_ids[1]);
}

TEST_F(FileManagerFileTasksComplexTest, FindAllTypesOfTasks_GoogleDocument) {
  // kFooId and kBarId copied from FindFileHandlerTasks test above.
  const char kBarId[] = "odlhccgofgkadkkhcmhgnhgahonahoca";

  // Bar.app can handle ".gdoc" files.
  // This is an extension (file browser handler).
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
                                                    .Append("filesystem:*.gdoc")
                                                    .Build())
                           .Build())
                   .Build())
          .Build());
  bar_app.SetID(kBarId);
  extension_service_->AddExtension(bar_app.Build().get());

  // The Files app can handle ".gdoc" files.
  // The ID "kFileManagerAppId" used here is precisely the one that identifies
  // the Chrome OS Files app application.
  extensions::ExtensionBuilder files_app;
  files_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Files")
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
                                                    .Append("filesystem:*.gdoc")
                                                    .Build())
                           .Build())
                   .Build())
          .Build());
  files_app.SetID(kFileManagerAppId);
  extension_service_->AddExtension(files_app.Build().get());

  // Find apps for a ".gdoc file". Only the built-in handler of the Files apps
  // should be found.
  std::vector<extensions::EntryInfo> entries;
  std::vector<GURL> file_urls;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.gdoc"),
      "application/vnd.google-apps.document", false);
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.gdoc");

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kFileManagerAppId, tasks[0].task_descriptor().app_id);
}

TEST_F(FileManagerFileTasksComplexTest, FindFileHandlerTask_Generic) {
  // Since we want to keep the order of the result as foo,bar,baz,qux,
  // keep the ids in alphabetical order.
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";
  const char kBarId[] = "odlhccgofgkadkkhcmhgnhgahonahoca";
  const char kBazId[] = "plifkpkakemokpflgbnnigcoldgcbdmc";
  const char kQuxId[] = "pmifkpkakgkadkkhcmhgnigmmifgpaph";

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
                  .Set("text", extensions::DictionaryBuilder()
                                   .Set("types", extensions::ListBuilder()
                                                     .Append("text/plain")
                                                     .Build())
                                   .Build())
                  .Build())
          .Build());
  foo_app.SetID(kFooId);
  extension_service_->AddExtension(foo_app.Build().get());

  // Bar app provides file handler for .txt and not provide generic file
  // handler, but handles directories.
  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Bar")
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
                  .Set("text",
                       extensions::DictionaryBuilder()
                           .Set("include_directories", true)
                           .Set("extensions",
                                extensions::ListBuilder().Append("txt").Build())
                           .Build())
                  .Build())
          .Build());
  bar_app.SetID(kBarId);
  extension_service_->AddExtension(bar_app.Build().get());

  // Baz app provides file handler for all extensions and images.
  extensions::ExtensionBuilder baz_app;
  baz_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Baz")
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
          .Set("file_handlers",
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
  baz_app.SetID(kBazId);
  extension_service_->AddExtension(baz_app.Build().get());

  // Qux app provides file handler for all types.
  extensions::ExtensionBuilder qux_app;
  qux_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Qux")
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
          .Set("file_handlers",
               extensions::DictionaryBuilder()
                   .Set("any",
                        extensions::DictionaryBuilder()
                            .Set("types",
                                 extensions::ListBuilder().Append("*").Build())
                            .Build())
                   .Build())
          .Build());
  qux_app.SetID(kQuxId);
  extension_service_->AddExtension(qux_app.Build().get());

  // Test case with .txt file
  std::vector<extensions::EntryInfo> txt_entries;
  txt_entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.txt"),
      "text/plain", false);
  std::vector<FullTaskDescriptor> txt_result;
  FindFileHandlerTasks(&test_profile_, txt_entries, &txt_result);
  EXPECT_EQ(4U, txt_result.size());
  // Foo app provides a handler for text/plain.
  EXPECT_EQ("Foo", txt_result[0].task_title());
  EXPECT_FALSE(txt_result[0].is_generic_file_handler());
  // Bar app provides a handler for .txt.
  EXPECT_EQ("Bar", txt_result[1].task_title());
  EXPECT_FALSE(txt_result[1].is_generic_file_handler());
  // Baz app provides a handler for all extensions.
  EXPECT_EQ("Baz", txt_result[2].task_title());
  EXPECT_TRUE(txt_result[2].is_generic_file_handler());
  // Qux app provides a handler for all types.
  EXPECT_EQ("Qux", txt_result[3].task_title());
  EXPECT_TRUE(txt_result[3].is_generic_file_handler());

  // Test case with .jpg file
  std::vector<extensions::EntryInfo> jpg_entries;
  jpg_entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.jpg"),
      "image/jpeg", false);
  std::vector<FullTaskDescriptor> jpg_result;
  FindFileHandlerTasks(&test_profile_, jpg_entries, &jpg_result);
  EXPECT_EQ(3U, jpg_result.size());
  // Foo app provides a handler for all types.
  EXPECT_EQ("Foo", jpg_result[0].task_title());
  EXPECT_TRUE(jpg_result[0].is_generic_file_handler());
  // Baz app provides a handler for image/*. A partial wildcarded handler is
  // treated as non-generic handler.
  EXPECT_EQ("Baz", jpg_result[1].task_title());
  EXPECT_FALSE(jpg_result[1].is_generic_file_handler());
  // Qux app provides a handler for all types.
  EXPECT_EQ("Qux", jpg_result[2].task_title());
  EXPECT_TRUE(jpg_result[2].is_generic_file_handler());

  // Test case with directories.
  std::vector<extensions::EntryInfo> dir_entries;
  dir_entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("dir"), "",
      true);
  std::vector<FullTaskDescriptor> dir_result;
  FindFileHandlerTasks(&test_profile_, dir_entries, &dir_result);
  ASSERT_EQ(1U, dir_result.size());
  // Confirm that only Bar.app is found and that it is a generic file handler.
  EXPECT_EQ(kBarId, dir_result[0].task_descriptor().app_id);
  EXPECT_TRUE(dir_result[0].is_generic_file_handler());
}

// The basic logic is similar to a test case for FindFileHandlerTasks above.
TEST_F(FileManagerFileTasksComplexTest, FindFileHandlerTask_Verbs) {
  // kFooId copied from FindFileHandlerTasks test above.
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";

  // Foo.app can handle "text/plain" and "text/html".
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
                                extensions::ListBuilder().Append("*").Build())
                           .Set("verb", "add_to")
                           .Build())
                  .Set("any_with_directories",
                       extensions::DictionaryBuilder()
                           .Set("include_directories", true)
                           .Set("types",
                                extensions::ListBuilder().Append("*").Build())
                           .Set("verb", "pack_with")
                           .Build())
                  .Set("all_text", extensions::DictionaryBuilder()
                                       .Set("title", "Text")
                                       .Set("types", extensions::ListBuilder()
                                                         .Append("text/plain")
                                                         .Append("text/html")
                                                         .Build())
                                       .Set("verb", "add_to")
                                       .Build())
                  .Set("plain_text", extensions::DictionaryBuilder()
                                         .Set("title", "Plain")
                                         .Set("types", extensions::ListBuilder()
                                                           .Append("text/plain")
                                                           .Build())
                                         .Set("verb", "open_with")
                                         .Build())
                  .Set("html_text_duplicate_verb",
                       extensions::DictionaryBuilder()
                           .Set("title", "Html")
                           .Set("types", extensions::ListBuilder()
                                             .Append("text/html")
                                             .Build())
                           .Set("verb", "add_to")
                           .Build())
                  .Set("share_plain_text",
                       extensions::DictionaryBuilder()
                           .Set("title", "Share Plain")
                           .Set("types", extensions::ListBuilder()
                                             .Append("text/plain")
                                             .Build())
                           .Set("verb", "share_with")
                           .Build())
                  .Build())
          .Build());
  foo_app.SetID(kFooId);
  extension_service_->AddExtension(foo_app.Build().get());

  // Find app with corresponding verbs for a "text/plain" file.
  // Foo.app with ADD_TO, OPEN_WITH, PACK_WITH and SHARE_WITH should be found,
  // but only one ADD_TO that is not a generic handler will be taken into
  // account, even though there are 2 ADD_TO matches for "text/plain".
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.txt"),
      "text/plain", false);

  std::vector<FullTaskDescriptor> tasks;
  FindFileHandlerTasks(&test_profile_, entries, &tasks);

  ASSERT_EQ(4U, tasks.size());
  EXPECT_EQ(kFooId, tasks[0].task_descriptor().app_id);
  EXPECT_EQ("Foo", tasks[0].task_title());
  EXPECT_EQ(Verb::VERB_ADD_TO, tasks[0].task_verb());
  EXPECT_EQ(kFooId, tasks[1].task_descriptor().app_id);
  EXPECT_EQ("Foo", tasks[1].task_title());
  EXPECT_EQ(Verb::VERB_OPEN_WITH, tasks[1].task_verb());
  EXPECT_EQ(kFooId, tasks[2].task_descriptor().app_id);
  EXPECT_EQ("Foo", tasks[2].task_title());
  EXPECT_EQ(Verb::VERB_PACK_WITH, tasks[2].task_verb());
  EXPECT_EQ(kFooId, tasks[3].task_descriptor().app_id);
  EXPECT_EQ("Foo", tasks[3].task_title());
  EXPECT_EQ(Verb::VERB_SHARE_WITH, tasks[3].task_verb());

  // Find app with corresponding verbs for a "text/html" file.
  // Foo.app with ADD_TO and PACK_WITH should be found, but only the first
  // ADD_TO that is a good match will be taken into account, even though there
  // are 3 ADD_TO matches for "text/html".
  entries.clear();
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("foo.html"),
      "text/html", false);
  tasks.clear();
  FindFileHandlerTasks(&test_profile_, entries, &tasks);

  ASSERT_EQ(2U, tasks.size());
  EXPECT_EQ(kFooId, tasks[0].task_descriptor().app_id);
  EXPECT_EQ("Foo", tasks[0].task_title());
  EXPECT_EQ(Verb::VERB_ADD_TO, tasks[0].task_verb());
  EXPECT_EQ(kFooId, tasks[1].task_descriptor().app_id);
  EXPECT_EQ("Foo", tasks[1].task_title());
  EXPECT_EQ(Verb::VERB_PACK_WITH, tasks[1].task_verb());

  // Find app with corresponding verbs for directories.
  // Foo.app with only PACK_WITH should be found.
  entries.clear();
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(&test_profile_).AppendASCII("dir"), "",
      true);
  tasks.clear();
  FindFileHandlerTasks(&test_profile_, entries, &tasks);

  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kFooId, tasks[0].task_descriptor().app_id);
  EXPECT_EQ("Foo", tasks[0].task_title());
  EXPECT_EQ(Verb::VERB_PACK_WITH, tasks[0].task_verb());
}

// Test using the test extension system, which needs lots of setup.
class FileManagerFileTasksCrostiniTest
    : public FileManagerFileTasksComplexTest {
 protected:
  FileManagerFileTasksCrostiniTest()
      : crostini_test_helper_(&test_profile_),
        crostini_folder_(util::GetCrostiniMountDirectory(&test_profile_)) {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetConciergeClient(
        std::make_unique<chromeos::FakeConciergeClient>());

    vm_tools::apps::App text_app =
        crostini::CrostiniTestHelper::BasicApp("text_app");
    *text_app.add_mime_types() = "text/plain";
    crostini_test_helper_.AddApp(text_app);

    vm_tools::apps::App image_app =
        crostini::CrostiniTestHelper::BasicApp("image_app");
    *image_app.add_mime_types() = "image/gif";
    *image_app.add_mime_types() = "image/jpeg";
    *image_app.add_mime_types() = "image/jpg";
    *image_app.add_mime_types() = "image/png";
    crostini_test_helper_.AddApp(image_app);

    vm_tools::apps::App gif_app =
        crostini::CrostiniTestHelper::BasicApp("gif_app");
    *gif_app.add_mime_types() = "image/gif";
    crostini_test_helper_.AddApp(gif_app);

    vm_tools::apps::App alt_mime_app =
        crostini::CrostiniTestHelper::BasicApp("alt_mime_app");
    *alt_mime_app.add_mime_types() = "foo/x-bar";
    crostini_test_helper_.AddApp(alt_mime_app);

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

    crostini::CrostiniMimeTypesServiceFactory::GetForProfile(&test_profile_)
        ->UpdateMimeTypes(mime_types_list);
  }

  crostini::CrostiniTestHelper crostini_test_helper_;
  base::FilePath crostini_folder_;
  std::string text_app_id_;
  std::string image_app_id_;
  std::string gif_app_id_;
  std::string alt_mime_app_id_;
};

TEST_F(FileManagerFileTasksCrostiniTest, BasicFiles) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("foo.txt"), "text/plain", false}};
  std::vector<GURL> file_urls{
      GURL("filesystem:chrome-extension://id/dir/foo.txt")};

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(text_app_id_, tasks[0].task_descriptor().app_id);

  // Multiple text files
  entries.emplace_back(crostini_folder_.Append("bar.txt"), "text/plain", false);
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/bar.txt");
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(text_app_id_, tasks[0].task_descriptor().app_id);
}

TEST_F(FileManagerFileTasksCrostiniTest, Directories) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("dir"), "", true}};
  std::vector<GURL> file_urls{GURL("filesystem:chrome-extension://id/dir/dir")};
  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  EXPECT_EQ(0U, tasks.size());

  entries.emplace_back(crostini_folder_.Append("foo.txt"), "text/plain", false);
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/foo.txt");
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  EXPECT_EQ(0U, tasks.size());
}

TEST_F(FileManagerFileTasksCrostiniTest, MultipleMatches) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("foo.gif"), "image/gif", false},
      {crostini_folder_.Append("bar.gif"), "image/gif", false}};
  std::vector<GURL> file_urls{
      GURL("filesystem:chrome-extension://id/dir/foo.gif"),
      GURL("filesystem:chrome-extension://id/dir/bar.gif")};

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  // The returned values happen to be ordered alphabetically by app_id, so we
  // rely on this to keep the test simple.
  EXPECT_LT(gif_app_id_, image_app_id_);
  ASSERT_EQ(2U, tasks.size());
  EXPECT_EQ(gif_app_id_, tasks[0].task_descriptor().app_id);
  EXPECT_EQ(image_app_id_, tasks[1].task_descriptor().app_id);
}

TEST_F(FileManagerFileTasksCrostiniTest, MultipleTypes) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("foo.gif"), "image/gif", false},
      {crostini_folder_.Append("bar.png"), "image/png", false}};
  std::vector<GURL> file_urls{
      GURL("filesystem:chrome-extension://id/dir/foo.gif"),
      GURL("filesystem:chrome-extension://id/dir/bar.png")};

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(image_app_id_, tasks[0].task_descriptor().app_id);

  entries.emplace_back(crostini_folder_.Append("qux.mp4"), "video/mp4", false);
  file_urls.emplace_back("filesystem:chrome-extension://id/dir/qux.mp4");
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  EXPECT_EQ(0U, tasks.size());
}

TEST_F(FileManagerFileTasksCrostiniTest, AlternateMimeTypes) {
  std::vector<extensions::EntryInfo> entries{
      {crostini_folder_.Append("bar1.foo"), "text/plain", false},
      {crostini_folder_.Append("bar2.foo"), "application/octet-stream", false}};
  std::vector<GURL> file_urls{
      GURL("filesystem:chrome-extension://id/dir/bar1.foo"),
      GURL("filesystem:chrome-extension://id/dir/bar2.foo")};

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasksSynchronousWrapper().Call(&test_profile_, entries,
                                               file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(alt_mime_app_id_, tasks[0].task_descriptor().app_id);
}

}  // namespace file_tasks
}  // namespace file_manager.
