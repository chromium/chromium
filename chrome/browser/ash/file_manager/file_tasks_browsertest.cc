// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/common/constants.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/features.h"

using web_app::kMediaAppId;

namespace file_manager {
namespace file_tasks {
namespace {

// A list of file extensions (`/` delimited) representing a selection of files
// and the app expected to be the default to open these files.
// A null app_id indicates there is no preferred default.
// A mime_type can be set to a result normally given by sniffing when
// net::GetMimeTypeFromFile() would not provide a result.
struct Expectation {
  const char* file_extensions;
  const char* app_id;
  const char* mime_type = nullptr;
};

// Verifies that a single default task expectation (i.e. the expected
// default app to open a given set of file extensions) matches the default
// task in a vector of task descriptors. Decrements the provided |remaining|
// integer to provide additional verification that this function is invoked
// an expected number of times (i.e. even if the callback could be invoked
// asynchronously).
void VerifyTasks(int* remaining,
                 Expectation expectation,
                 std::unique_ptr<std::vector<FullTaskDescriptor>> result) {
  ASSERT_TRUE(result) << expectation.file_extensions;
  --*remaining;

  auto default_task =
      std::find_if(result->begin(), result->end(),
                   [](const auto& task) { return task.is_default; });

  // Early exit for the uncommon situation where no default should be set.
  if (!expectation.app_id) {
    EXPECT_TRUE(default_task == result->end()) << expectation.file_extensions;
    return;
  }

  ASSERT_TRUE(default_task != result->end()) << expectation.file_extensions;

  EXPECT_EQ(expectation.app_id, default_task->task_descriptor.app_id)
      << " for extension: " << expectation.file_extensions;

  // Verify no other task is set as default.
  EXPECT_EQ(1, std::count_if(result->begin(), result->end(),
                             [](const auto& task) { return task.is_default; }))
      << expectation.file_extensions;
}

// Helper to quit a run loop after invoking VerifyTasks().
void VerifyAsyncTask(int* remaining,
                     Expectation expectation,
                     base::OnceClosure quit_closure,
                     std::unique_ptr<std::vector<FullTaskDescriptor>> result) {
  VerifyTasks(remaining, expectation, std::move(result));
  std::move(quit_closure).Run();
}

// Installs a chrome app that handles .tiff.
scoped_refptr<const extensions::Extension> InstallTiffHandlerChromeApp(
    Profile* profile) {
  return test::InstallTestingChromeApp(
      profile, "extensions/api_test/file_browser/app_file_handler");
}

class FileTasksBrowserTestBase
    : public TestProfileTypeMixin<InProcessBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    test::AddDefaultComponentExtensionsOnMainThread(browser()->profile());
    web_app::WebAppProvider::GetForTest(browser()->profile())
        ->system_web_app_manager()
        .InstallSystemAppsForTesting();
  }

  // Tests that each of the passed expectations open by default in the expected
  // app.
  void TestExpectationsAgainstDefaultTasks(
      const std::vector<Expectation>& expectations) {
    int remaining = expectations.size();
    const base::FilePath prefix = base::FilePath().AppendASCII("file");

    for (const Expectation& test : expectations) {
      std::vector<extensions::EntryInfo> entries;
      std::vector<GURL> file_urls;
      std::vector<base::StringPiece> all_extensions =
          base::SplitStringPiece(test.file_extensions, "/",
                                 base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      for (base::StringPiece extension : all_extensions) {
        base::FilePath path = prefix.AddExtension(extension);
        std::string mime_type;
        net::GetMimeTypeFromFile(path, &mime_type);
        if (test.mime_type != nullptr) {
          // Sniffing isn't used when GetMimeTypeFromFile() succeeds, so there
          // shouldn't be a hard-coded mime type configured.
          EXPECT_TRUE(mime_type.empty());
          mime_type = test.mime_type;
        } else {
          EXPECT_FALSE(mime_type.empty()) << "No mime type for " << path;
        }
        entries.push_back({path, mime_type, false});
        GURL url = GURL(base::JoinString(
            {"filesystem:https://site.com/isolated/foo.", extension}, ""));
        ASSERT_TRUE(url.is_valid());
        file_urls.push_back(url);
      }

      // task_verifier callback is invoked synchronously from
      // FindAllTypesOfTasks.
      FindAllTypesOfTasks(browser()->profile(), entries, file_urls,
                          base::BindOnce(&VerifyTasks, &remaining, test));
    }
    EXPECT_EQ(0, remaining);
  }

  // PDF handler expectations when |kMediaAppHandlesPdf| is off (the default).
  std::vector<Expectation> GetDefaultPdfExpectations() {
    const char* file_manager_app_id = ash::features::IsFileManagerSwaEnabled()
                                          ? kFileManagerSwaAppId
                                          : kFileManagerAppId;
    return {{"pdf", file_manager_app_id}, {"PDF", file_manager_app_id}};
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kFileHandlingAPI};
};

class FileTasksBrowserTest : public FileTasksBrowserTestBase {
 public:
  FileTasksBrowserTest() {
    // Enable Media App without PDF support.
    scoped_feature_list_.InitWithFeatures({},
                                          {ash::features::kMediaAppHandlesPdf});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FileTasksBrowserTestWithPdf : public FileTasksBrowserTestBase {
 public:
  FileTasksBrowserTestWithPdf() {
    // Enable Media App PDF support.
    scoped_feature_list_.InitWithFeatures({ash::features::kMediaAppHandlesPdf},
                                          {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// List of single file default app expectations. Changes to this test may have
// implications for file handling declarations in built-in app manifests,
// because logic in ChooseAndSetDefaultTask() treats handlers for extensions
// with a higher priority than handlers for mime types. Provide MIME types here
// for extensions known to be missing mime types from net::GetMimeTypeFromFile()
// (see ExtensionToMimeMapping test). In practice, these MIME types are
// populated via file sniffing, but tests in this file do not operate on real
// files. We hard code MIME types that file sniffing obtained experimentally
// from sample files.

constexpr Expectation kAudioExpectations[] = {
    {"flac", kMediaAppId}, {"m4a", kMediaAppId}, {"mp3", kMediaAppId},
    {"oga", kMediaAppId},  {"ogg", kMediaAppId}, {"wav", kMediaAppId},
};

constexpr Expectation kVideoExpectations[] = {
    {"3gp", kMediaAppId, "application/octet-stream"},
    {"avi", kMediaAppId, "application/octet-stream"},
    {"m4v", kMediaAppId},
    {"mkv", kMediaAppId, "video/webm"},
    {"mov", kMediaAppId, "application/octet-stream"},
    {"mp4", kMediaAppId},
    {"mpeg", kMediaAppId},
    {"mpeg4", kMediaAppId, "video/mpeg"},
    {"mpg", kMediaAppId},
    {"mpg4", kMediaAppId, "video/mpeg"},
    {"ogm", kMediaAppId},
    {"ogv", kMediaAppId},
    {"ogx", kMediaAppId, "video/ogg"},
    {"webm", kMediaAppId},
};

// PDF handler expectations when |kMediaAppHandlesPdf| is on.
constexpr Expectation kMediaAppPdfExpectations[] = {{"pdf", kMediaAppId},
                                                    {"PDF", kMediaAppId}};

}  // namespace

// Test file extensions correspond to mime types where expected.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, ExtensionToMimeMapping) {
  constexpr struct {
    const char* file_extension;
    bool has_mime = true;
  } kExpectations[] = {
      // Images.
      {"bmp"},
      {"gif"},
      {"ico"},
      {"jpg"},
      {"jpeg"},
      {"png"},
      {"webp"},

      // Raw.
      {"arw", false},
      {"cr2", false},
      {"dng", false},
      {"nef", false},
      {"nrw", false},
      {"orf", false},
      {"raf", false},
      {"rw2", false},

      // Video.
      {"3gp", false},
      {"avi", false},
      {"m4v"},
      {"mkv", false},
      {"mov", false},
      {"mp4"},
      {"mpeg"},
      {"mpeg4", false},
      {"mpg"},
      {"mpg4", false},
      {"ogm"},
      {"ogv"},
      {"ogx", false},
      {"webm"},

      // Audio.
      {"amr", false},
      {"flac"},
      {"m4a"},
      {"mp3"},
      {"oga"},
      {"ogg"},
      {"wav"},
  };

  const base::FilePath prefix = base::FilePath().AppendASCII("file");
  std::string mime_type;

  for (const auto& test : kExpectations) {
    base::FilePath path = prefix.AddExtension(test.file_extension);

    EXPECT_EQ(test.has_mime, net::GetMimeTypeFromFile(path, &mime_type))
        << test.file_extension;
  }
}

// Tests the default handlers for various file types in ChromeOS. This test
// exists to ensure the default app that launches when you open a file in the
// ChromeOS file manager does not change unexpectedly. Multiple default apps are
// allowed to register a handler for the same file type. Without that, it is not
// possible for an app to open that type even when given explicit direction via
// the chrome.fileManagerPrivate.executeTask API. The current conflict
// resolution mechanism is "sort by extension ID", which has the desired result.
// If desires change, we'll need to update ChooseAndSetDefaultTask() with some
// additional logic.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, DefaultHandlerChangeDetector) {
  // Media App should handle images, video and audio by default.
  std::vector<Expectation> expectations = {
      // Images.
      {"bmp", kMediaAppId},
      {"gif", kMediaAppId},
      {"ico", kMediaAppId},
      {"jpg", kMediaAppId},
      {"jpeg", kMediaAppId},
      {"png", kMediaAppId},
      {"webp", kMediaAppId},
      // Raw (handled by MediaApp).
      {"arw", kMediaAppId, "image/tiff"},
      {"cr2", kMediaAppId, "image/tiff"},
      {"dng", kMediaAppId, "image/tiff"},
      {"nef", kMediaAppId, "image/tiff"},
      {"nrw", kMediaAppId, "image/tiff"},
      {"orf", kMediaAppId, "image/tiff"},
      {"raf", kMediaAppId, "image/tiff"},
      {"rw2", kMediaAppId, "image/tiff"},
      {"NRW", kMediaAppId, "image/tiff"},  // Uppercase extension.
      {"arw", kMediaAppId, ""},  // Missing MIME type (unable to sniff).
  };
  expectations.insert(expectations.end(), std::begin(kVideoExpectations),
                      std::end(kVideoExpectations));
  expectations.insert(expectations.end(), std::begin(kAudioExpectations),
                      std::end(kAudioExpectations));

  auto pdf_expectations = GetDefaultPdfExpectations();
  expectations.insert(expectations.end(), std::begin(pdf_expectations),
                      std::end(pdf_expectations));

  TestExpectationsAgainstDefaultTasks(expectations);
}

// Tests the default handlers that are different with PDF support enabled.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTestWithPdf, PdfHandlerChangeDetector) {
  std::vector<Expectation> expectations(std::begin(kMediaAppPdfExpectations),
                                        std::end(kMediaAppPdfExpectations));
  TestExpectationsAgainstDefaultTasks(expectations);
}

// Spot test the default handlers for selections that include multiple different
// file types. Only tests combinations of interest to the Media App.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, MultiSelectDefaultHandler) {
  std::vector<Expectation> expectations = {
      {"jpg/gif", kMediaAppId},
      {"jpg/mp4", kMediaAppId},
  };

  TestExpectationsAgainstDefaultTasks(expectations);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Check that QuickOffice has a handler installed for common Office doc types.
// This test only runs with the is_chrome_branded GN flag set because otherwise
// QuickOffice is not installed.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, QuickOffice) {
  std::vector<Expectation> expectations = {
      {"doc", extension_misc::kQuickOfficeComponentExtensionId,
       "application/msword"},
      {"docx", extension_misc::kQuickOfficeComponentExtensionId,
       "application/"
       "vnd.openxmlformats-officedocument.wordprocessingml.document"},
      {"ppt", extension_misc::kQuickOfficeComponentExtensionId,
       "application/vnd.ms-powerpoint"},
      {"pptx", extension_misc::kQuickOfficeComponentExtensionId,
       "application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation"},
      {"xls", extension_misc::kQuickOfficeComponentExtensionId},
      {"xlsx", extension_misc::kQuickOfficeComponentExtensionId},
  };

  TestExpectationsAgainstDefaultTasks(expectations);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// The Media App will be preferred over a chrome app with a specific extension,
// unless that app is set default via prefs.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, MediaAppPreferredOverChromeApps) {
  if (profile_type() == TestProfileType::kGuest) {
    // The provided file system can't install in guest mode. Just check that
    // MediaApp handles tiff.
    TestExpectationsAgainstDefaultTasks({{"tiff", kMediaAppId}});
    return;
  }
  Profile* profile = browser()->profile();
  auto extension = InstallTiffHandlerChromeApp(profile);
  TestExpectationsAgainstDefaultTasks({{"tiff", kMediaAppId}});

  UpdateDefaultTask(
      profile->GetPrefs(),
      TaskDescriptor(extension->id(), StringToTaskType("app"), "tiffAction"),
      {"tiff"}, {"image/tiff"});
  if (profile_type() == TestProfileType::kIncognito) {
    // In incognito, the installed app is not enabled and we filter it out.
    TestExpectationsAgainstDefaultTasks({{"tiff", kMediaAppId}});
  } else {
    TestExpectationsAgainstDefaultTasks({{"tiff", extension->id().c_str()}});
  }
}

// Test expectations for files coming from provided file systems.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, ProvidedFileSystemFileSource) {
  if (profile_type() == TestProfileType::kGuest) {
    // Provided file systems don't exist in guest. This test seems to work OK in
    // incognito mode though.
    return;
  }
  // The current test expectation: a GIF file in the provided file system called
  // "readwrite.gif" should open with the MediaApp.
  const char kTestFile[] = "readwrite.gif";
  Expectation test = {"gif", kMediaAppId};
  int remaining_expectations = 1;

  Profile* profile = browser()->profile();
  base::WeakPtr<Volume> volume =
      test::InstallFileSystemProviderChromeApp(profile);

  GURL url;
  ASSERT_TRUE(util::ConvertAbsoluteFilePathToFileSystemUrl(
      profile, volume->mount_path().AppendASCII(kTestFile),
      util::GetFileManagerURL(), &url));

  // Note |url| differs slightly to the result of ToGURL() below. The colons
  // either side of `:test-image-provider-fs:` become escaped as `%3A`.

  storage::FileSystemURL filesystem_url =
      util::GetFileManagerFileSystemContext(profile)
          ->CrackURLInFirstPartyContext(url);

  std::vector<GURL> urls = {filesystem_url.ToGURL()};
  std::vector<extensions::EntryInfo> entries;

  // We could add the mime type here, but since a "real" file is provided, we
  // can get additional coverage of the mime determination. For non-native files
  // this uses metadata only (not sniffing).
  entries.emplace_back(filesystem_url.path(), "", false);

  base::RunLoop run_loop;
  auto verifier = base::BindOnce(&VerifyAsyncTask, &remaining_expectations,
                                 test, run_loop.QuitClosure());
  extensions::app_file_handler_util::GetMimeTypeForLocalPath(
      profile, entries[0].path,
      base::BindLambdaForTesting([&](const std::string& mime_type) {
        entries[0].mime_type = mime_type;
        EXPECT_EQ(entries[0].mime_type, "image/gif");
        FindAllTypesOfTasks(profile, entries, urls, std::move(verifier));
      }));
  run_loop.Run();
  EXPECT_EQ(remaining_expectations, 0);
}

IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, ExecuteWebApp) {
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = GURL("https://www.example.com/");
  web_app_info->scope = GURL("https://www.example.com/");
  apps::FileHandler handler;
  handler.action = GURL("https://www.example.com/handle_file");
  handler.display_name = u"activity name";
  apps::FileHandler::AcceptEntry accept_entry1;
  accept_entry1.mime_type = "image/jpeg";
  accept_entry1.file_extensions.insert(".jpeg");
  handler.accept.push_back(accept_entry1);
  apps::FileHandler::AcceptEntry accept_entry2;
  accept_entry2.mime_type = "image/png";
  accept_entry2.file_extensions.insert(".png");
  handler.accept.push_back(accept_entry2);
  web_app_info->file_handlers.push_back(std::move(handler));

  Profile* const profile = browser()->profile();
  TaskDescriptor task_descriptor;
  if (GetParam().crosapi_state == TestProfileParam::CrosapiParam::kDisabled) {
    // Install a PWA in ash.
    web_app::AppId app_id =
        web_app::test::InstallWebApp(profile, std::move(web_app_info));
    task_descriptor = TaskDescriptor(app_id, TaskType::TASK_TYPE_WEB_APP,
                                     "https://www.example.com/handle_file");
    // Skip past the permission dialog.
    web_app::WebAppProvider::GetForTest(profile)
        ->sync_bridge()
        .SetAppFileHandlerApprovalState(app_id,
                                        web_app::ApiApprovalState::kAllowed);
  } else {
    // Use an existing SWA in ash - Media app.
    task_descriptor = TaskDescriptor(kMediaAppId, TaskType::TASK_TYPE_WEB_APP,
                                     "chrome://media-app/open");
    // TODO(petermarshall): Install the web app in Lacros once installing and
    // launching apps from ash -> lacros is possible.
  }

  base::RunLoop run_loop;
  web_app::WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
      base::BindLambdaForTesting(
          [&run_loop](apps::AppLaunchParams&& params) -> content::WebContents* {
            if (GetParam().crosapi_state ==
                TestProfileParam::CrosapiParam::kDisabled) {
              EXPECT_EQ(params.override_url,
                        "https://www.example.com/handle_file");
            } else {
              EXPECT_EQ(params.override_url, "chrome://media-app/open");
            }
            EXPECT_EQ(params.launch_files.size(), 2U);
            EXPECT_TRUE(base::EndsWith(params.launch_files.at(0).MaybeAsASCII(),
                                       "foo.jpeg"));
            EXPECT_TRUE(base::EndsWith(params.launch_files.at(1).MaybeAsASCII(),
                                       "bar.png"));
            run_loop.Quit();
            return nullptr;
          }));

  base::FilePath file1 =
      util::GetMyFilesFolderForProfile(profile).AppendASCII("foo.jpeg");
  base::FilePath file2 =
      util::GetMyFilesFolderForProfile(profile).AppendASCII("bar.png");
  GURL url1;
  CHECK(util::ConvertAbsoluteFilePathToFileSystemUrl(
      profile, file1, util::GetFileManagerURL(), &url1));
  GURL url2;
  CHECK(util::ConvertAbsoluteFilePathToFileSystemUrl(
      profile, file2, util::GetFileManagerURL(), &url2));

  std::vector<storage::FileSystemURL> files;
  files.push_back(storage::FileSystemURL::CreateForTest(url1));
  files.push_back(storage::FileSystemURL::CreateForTest(url2));
  ExecuteFileTask(profile, task_descriptor, files, base::DoNothing());
  run_loop.Run();
}

// Launch a Chrome app with a real file and wait for it to ping back.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, ExecuteChromeApp) {
  if (profile_type() == TestProfileType::kGuest) {
    // The app can't install in guest mode.
    return;
  }
  Profile* const profile = browser()->profile();
  auto extension = InstallTiffHandlerChromeApp(profile);

  TaskDescriptor task_descriptor(extension->id(), TASK_TYPE_FILE_HANDLER,
                                 "tiffAction");

  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("chromeos/file_manager/test_small.tiff");
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(path));
  }
  // Copy the file into My Files.
  file_manager::test::FolderInMyFiles folder(profile);
  folder.Add({path});
  base::FilePath path_in_my_files = folder.files()[0];

  GURL tiff_url;
  CHECK(util::ConvertAbsoluteFilePathToFileSystemUrl(
      profile, path_in_my_files, util::GetFileManagerURL(), &tiff_url));
  std::vector<storage::FileSystemURL> files;
  files.push_back(storage::FileSystemURL::CreateForTest(tiff_url));

  content::DOMMessageQueue message_queue;
  ExecuteFileTask(profile, task_descriptor, files, base::DoNothing());

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  ASSERT_EQ("\"Received tiffAction with: test_small.tiff\"", message);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    FileTasksBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    FileTasksBrowserTestWithPdf);

}  // namespace file_tasks
}  // namespace file_manager
