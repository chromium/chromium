// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <unordered_map>

#include "ash/webui/file_manager/url_constants.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/drive/file_errors.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/extension_registry.h"
#include "net/base/mime_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

using web_app::kMediaAppId;

namespace file_manager {
namespace file_tasks {
namespace {

const char* blockedUrl = "https://blocked.com";

static const char kODFSSampleUrl[] = "https://1drv.ms/123";
static const char kSampleUserEmail1[] = "user1@gmail.com";
static const char kSampleUserEmail2[] = "user2@gmail.com";

// A list of file extensions (`/` delimited) representing a selection of files
// and the app expected to be the default to open these files.
// A null app_id indicates there is no preferred default.
// A mime_type can be set to a result normally given by sniffing when
// net::GetMimeTypeFromFile() would not provide a result.
// A source_url can be set when testing DLP restrictions and is also used to
// determine whether fetched tasks should be blocked. If null, the task should
// never be blocked.
struct Expectation {
  const char* file_extensions;
  const char* app_id;
  const char* mime_type = nullptr;
  const char* dlp_source_url = nullptr;
};

// Verifies that a single default task expectation (i.e. the expected
// default app to open a given set of file extensions) matches the default
// task in a vector of task descriptors. Decrements the provided |remaining|
// integer to provide additional verification that this function is invoked
// an expected number of times (i.e. even if the callback could be invoked
// asynchronously).
void VerifyTasks(int* remaining,
                 Expectation expectation,
                 std::unique_ptr<ResultingTasks> resulting_tasks) {
  ASSERT_TRUE(resulting_tasks) << expectation.file_extensions;
  --*remaining;

  auto default_task = base::ranges::find_if(resulting_tasks->tasks,
                                            &FullTaskDescriptor::is_default);

  // Early exit for the uncommon situation where no default should be set.
  if (!expectation.app_id) {
    EXPECT_TRUE(default_task == resulting_tasks->tasks.end())
        << expectation.file_extensions;
    return;
  }

  ASSERT_TRUE(default_task != resulting_tasks->tasks.end())
      << expectation.file_extensions;

  EXPECT_EQ(expectation.app_id, default_task->task_descriptor.app_id)
      << " for extension: " << expectation.file_extensions;

  // Verify no other task is set as default.
  EXPECT_EQ(1, base::ranges::count_if(resulting_tasks->tasks,
                                      &FullTaskDescriptor::is_default))
      << expectation.file_extensions;
}

// Helper to quit a run loop after invoking VerifyTasks().
void VerifyAsyncTask(int* remaining,
                     Expectation expectation,
                     base::OnceClosure quit_closure,
                     std::unique_ptr<ResultingTasks> resulting_tasks) {
  VerifyTasks(remaining, expectation, std::move(resulting_tasks));
  std::move(quit_closure).Run();
}

// Verifies that all tasks are either blocked or not by DLP, according to
// |expectation|. Decrements the provided |remaining| integer to provide
// additional verification that this function is invoked an expected number of
// times (i.e. even if the callback could be invoked asynchronously).
void VerifyDlpStatus(int* remaining,
                     Expectation expectation,
                     std::unique_ptr<ResultingTasks> resulting_tasks) {
  ASSERT_TRUE(resulting_tasks) << expectation.file_extensions;
  --*remaining;

  bool expect_dlp_blocked = expectation.dlp_source_url &&
                            strcmp(expectation.dlp_source_url, blockedUrl) == 0;
  EXPECT_EQ(expect_dlp_blocked,
            base::ranges::all_of(resulting_tasks->tasks,
                                 &FullTaskDescriptor::is_dlp_blocked));
}

// Installs a chrome app that handles .tiff.
scoped_refptr<const extensions::Extension> InstallTiffHandlerChromeApp(
    Profile* profile) {
  return test::InstallTestingChromeApp(
      profile, "extensions/api_test/file_browser/app_file_handler");
}

// Populates |entries|, |file_urls|, and |dlp_source_urls| based on |test|.
void ConvertExpectation(const Expectation& test,
                        std::vector<extensions::EntryInfo>& entries,
                        std::vector<GURL>& file_urls,
                        std::vector<std::string>& dlp_source_urls) {
  const base::FilePath prefix = base::FilePath().AppendASCII("file");
  std::vector<base::StringPiece> all_extensions = base::SplitStringPiece(
      test.file_extensions, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (base::StringPiece extension : all_extensions) {
    base::FilePath path = prefix.AddExtension(extension);
    std::string mime_type;
    net::GetMimeTypeFromFile(path, &mime_type);
    if (test.mime_type != nullptr) {
      // Sniffing isn't used when GetMimeTypeFromFile() succeeds, so there
      // shouldn't be a hard-coded mime type configured.
      EXPECT_TRUE(mime_type.empty())
          << "Did not expect mime match " << mime_type << " for " << path;
      mime_type = test.mime_type;
    } else {
      EXPECT_FALSE(mime_type.empty()) << "No mime type for " << path;
    }
    entries.emplace_back(path, mime_type, false);
    GURL url = GURL(base::JoinString(
        {"filesystem:https://site.com/isolated/foo.", extension}, ""));
    ASSERT_TRUE(url.is_valid());
    file_urls.push_back(url);
    dlp_source_urls.push_back(test.dlp_source_url ? test.dlp_source_url : "");
  }
}

class FileTasksBrowserTest : public TestProfileTypeMixin<InProcessBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    TestProfileTypeMixin<InProcessBrowserTest>::SetUpOnMainThread();
    test::AddDefaultComponentExtensionsOnMainThread(browser()->profile());
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
  }

  // Tests that each of the passed expectations open by default in the expected
  // app.
  void TestExpectationsAgainstDefaultTasks(
      const std::vector<Expectation>& expectations) {
    int remaining = expectations.size();

    for (const Expectation& test : expectations) {
      std::vector<extensions::EntryInfo> entries;
      std::vector<GURL> file_urls;
      std::vector<std::string> dlp_source_urls;
      ConvertExpectation(test, entries, file_urls, dlp_source_urls);

      // task_verifier callback is invoked synchronously from
      // FindAllTypesOfTasks.
      FindAllTypesOfTasks(browser()->profile(), entries, file_urls,
                          dlp_source_urls,
                          base::BindOnce(&VerifyTasks, &remaining, test));
    }
    EXPECT_EQ(0, remaining);
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kFileHandlingAPI};
};

}  // namespace

// Changes to the following tests may have implications for file handling
// declarations in built-in app manifests, because logic in
// ChooseAndSetDefaultTask() treats handlers for extensions with a higher
// priority than handlers for mime types. Provide MIME types here for extensions
// known to be missing mime types from net::GetMimeTypeFromFile() (see
// ExtensionToMimeMapping test). In practice, these MIME types are populated via
// file sniffing, but tests in this file do not operate on real files. We hard
// code MIME types that file sniffing obtained experimentally from sample files.

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
      {"arw"},
      {"cr2"},
      {"dng"},
      {"nef"},
      {"nrw"},
      {"orf"},
      {"raf"},
      {"rw2"},

      // Video.
      {"3gp"},
      {"avi"},
      {"m4v"},
      {"mkv"},
      {"mov"},
      {"mp4"},
      {"mpeg"},
      {"mpeg4", false},
      {"mpg"},
      {"mpg4", false},
      {"ogm"},
      {"ogv"},
      {"ogx"},
      {"webm"},

      // Audio.
      {"amr"},
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
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, ImageHandlerChangeDetector) {
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
      {"arw", kMediaAppId},
      {"cr2", kMediaAppId},
      {"dng", kMediaAppId},
      {"nef", kMediaAppId},
      {"nrw", kMediaAppId},
      {"orf", kMediaAppId},
      {"raf", kMediaAppId},
      {"rw2", kMediaAppId},
      {"NRW", kMediaAppId},  // Uppercase extension.
  };
  TestExpectationsAgainstDefaultTasks(expectations);
}

IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, VideoHandlerChangeDetector) {
  std::vector<Expectation> expectations = {
      {"3gp", kMediaAppId},  {"avi", kMediaAppId},
      {"m4v", kMediaAppId},  {"mkv", kMediaAppId},
      {"mov", kMediaAppId},  {"mp4", kMediaAppId},
      {"mpeg", kMediaAppId}, {"mpeg4", kMediaAppId, "video/mpeg"},
      {"mpg", kMediaAppId},  {"mpg4", kMediaAppId, "video/mpeg"},
      {"ogm", kMediaAppId},  {"ogv", kMediaAppId},
      {"ogx", kMediaAppId},  {"webm", kMediaAppId},
  };
  TestExpectationsAgainstDefaultTasks(expectations);
}

IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, AudioHandlerChangeDetector) {
  std::vector<Expectation> expectations = {
      {"flac", kMediaAppId}, {"m4a", kMediaAppId}, {"mp3", kMediaAppId},
      {"oga", kMediaAppId},  {"ogg", kMediaAppId}, {"wav", kMediaAppId},
  };
  TestExpectationsAgainstDefaultTasks(expectations);
}

IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, PdfHandlerChangeDetector) {
  std::vector<Expectation> expectations = {{"pdf", kMediaAppId},
                                           {"PDF", kMediaAppId}};
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
  // TODO(b/287165243): Fix the test and remove this.
  if (GetParam().crosapi_state == TestProfileParam::CrosapiParam::kEnabled) {
    GTEST_SKIP()
        << "Skipping test body for CrosapiParam::kEnabled, see b/287165243.";
  }

  std::vector<Expectation> expectations = {
      {"doc", extension_misc::kQuickOfficeComponentExtensionId},
      {"docx", extension_misc::kQuickOfficeComponentExtensionId},
      {"ppt", extension_misc::kQuickOfficeComponentExtensionId},
      {"pptx", extension_misc::kQuickOfficeComponentExtensionId},
      {"xls", extension_misc::kQuickOfficeComponentExtensionId},
      {"xlsx", extension_misc::kQuickOfficeComponentExtensionId},
  };

  TestExpectationsAgainstDefaultTasks(expectations);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// The Media App will be preferred over a chrome app with a specific extension,
// unless that app is set default via prefs.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, MediaAppPreferredOverChromeApps) {
  // TODO(b/287165243): Fix the test and remove this.
  if (GetParam().crosapi_state == TestProfileParam::CrosapiParam::kEnabled) {
    GTEST_SKIP()
        << "Skipping test body for CrosapiParam::kEnabled, see b/287165243.";
  }

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
      profile,
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
  // TODO(b/287165243): Fix the test and remove this.
  if (GetParam().crosapi_state == TestProfileParam::CrosapiParam::kEnabled) {
    GTEST_SKIP()
        << "Skipping test body for CrosapiParam::kEnabled, see b/287165243.";
  }

  if (profile_type() == TestProfileType::kGuest) {
    // Provided file systems don't exist in guest.
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
        FindAllTypesOfTasks(profile, entries, urls, {""}, std::move(verifier));
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
        ->sync_bridge_unsafe()
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
  web_app::WebAppLaunchProcess::SetOpenApplicationCallbackForTesting(
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
  ExecuteFileTask(profile, task_descriptor, files, nullptr, base::DoNothing());
  run_loop.Run();
}

// Launch a Chrome app with a real file and wait for it to ping back.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, ExecuteChromeApp) {
  // TODO(b/287165243): Fix the test and remove this.
  if (GetParam().crosapi_state == TestProfileParam::CrosapiParam::kEnabled) {
    GTEST_SKIP()
        << "Skipping test body for CrosapiParam::kEnabled, see b/287165243.";
  }

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
  ExecuteFileTask(profile, task_descriptor, files, nullptr, base::DoNothing());

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  ASSERT_EQ("\"Received tiffAction with: test_small.tiff\"", message);
}

IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, IsExtensionInstalled) {
  // TODO(b/287165243): Fix the test and remove this.
  if (GetParam().crosapi_state == TestProfileParam::CrosapiParam::kEnabled) {
    GTEST_SKIP()
        << "Skipping test body for CrosapiParam::kEnabled, see b/287165243.";
  }

  if (profile_type() == TestProfileType::kGuest) {
    // The extension can't install in guest mode.
    return;
  }
  Profile* const profile = browser()->profile();
  // Install new extension.
  auto extension = InstallTiffHandlerChromeApp(profile);
  ASSERT_TRUE(IsExtensionInstalled(profile, extension->id()));

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  // Uninstall extension.
  registry->RemoveEnabled(extension->id());
  ASSERT_FALSE(IsExtensionInstalled(profile, extension->id()));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// This test only runs with the is_chrome_branded GN flag set because otherwise
// QuickOffice is not installed.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, IsExtensionInstalledQuickOffice) {
  // TODO(b/287165243): Fix the test and remove this.
  if (GetParam().crosapi_state == TestProfileParam::CrosapiParam::kEnabled) {
    GTEST_SKIP()
        << "Skipping test body for CrosapiParam::kEnabled, see b/287165243.";
  }

  Profile* const profile = browser()->profile();
  ASSERT_TRUE(IsExtensionInstalled(
      profile, extension_misc::kQuickOfficeComponentExtensionId));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

const TaskDescriptor CreateWebDriveOfficeTask() {
  // The SWA actionId is prefixed with chrome://file-manager/?ACTION_ID.
  const std::string& full_action_id =
      base::StrCat({ash::file_manager::kChromeUIFileManagerURL, "?",
                    kActionIdWebDriveOfficeWord});
  return TaskDescriptor(kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                        full_action_id);
}

const TaskDescriptor CreateOpenInOfficeTask() {
  // The SWA actionId is prefixed with chrome://file-manager/?ACTION_ID.
  const std::string& full_action_id = base::StrCat(
      {ash::file_manager::kChromeUIFileManagerURL, "?", kActionIdOpenInOffice});
  return TaskDescriptor(kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                        full_action_id);
}

const FileSystemURL CreateOfficeFileSourceURL(Profile* profile) {
  base::FilePath file =
      util::GetMyFilesFolderForProfile(profile).AppendASCII("text.docx");
  return ash::cloud_upload::FilePathToFileSystemURL(
      profile, file_manager::util::GetFileManagerFileSystemContext(profile),
      file);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// This test only runs with the is_chrome_branded GN flag set because otherwise
// QuickOffice is not installed.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, FallbackFailsNoQuickOffice) {
  // TODO(b/287165243): Fix the test and remove this.
  if (GetParam().crosapi_state == TestProfileParam::CrosapiParam::kEnabled) {
    GTEST_SKIP()
        << "Skipping test body for CrosapiParam::kEnabled, see b/287165243.";
  }

  storage::FileSystemURL test_url;
  Profile* const profile = browser()->profile();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* quick_office = registry->GetInstalledExtension(
      extension_misc::kQuickOfficeComponentExtensionId);

  // Uninstall QuickOffice.
  registry->RemoveEnabled(extension_misc::kQuickOfficeComponentExtensionId);
  // GetUserFallbackChoice() returns `False` because QuickOffice is not
  // installed.
  ASSERT_FALSE(GetUserFallbackChoice(
      profile, CreateWebDriveOfficeTask(), {test_url}, nullptr,
      ash::office_fallback::FallbackReason::kOffline));
  // Install QuickOffice.
  registry->AddEnabled(quick_office);
  // GetUserFallbackChoice() returns `True` because QuickOffice is installed.
  ASSERT_TRUE(GetUserFallbackChoice(
      profile, CreateWebDriveOfficeTask(), {test_url}, nullptr,
      ash::office_fallback::FallbackReason::kOffline));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Tests that apply DLP policies before fetching tasks and verify expectations
// on the blocked status.
class FileTasksPolicyBrowserTest : public FileTasksBrowserTest {
 public:
  // Tests that fetched tasks are marked as blocked by DLP, if expected.
  void TestExpectationsAgainstDlp(
      const std::vector<Expectation>& expectations) {
    int remaining = expectations.size();

    for (const Expectation& test : expectations) {
      std::vector<extensions::EntryInfo> entries;
      std::vector<GURL> file_urls;
      std::vector<std::string> dlp_source_urls;
      ConvertExpectation(test, entries, file_urls, dlp_source_urls);

      // task_verifier callback is invoked synchronously from
      // FindAllTypesOfTasks.
      FindAllTypesOfTasks(browser()->profile(), entries, file_urls,
                          dlp_source_urls,
                          base::BindOnce(&VerifyDlpStatus, &remaining, test));
    }
    EXPECT_EQ(0, remaining);
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>();
    rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

 protected:
  raw_ptr<policy::MockDlpRulesManager, ExperimentalAsh> rules_manager_ =
      nullptr;
};

IN_PROC_BROWSER_TEST_P(FileTasksPolicyBrowserTest, TasksMarkedAsBlocked) {
  // TODO(b/287165243): Fix the test and remove this.
  if (GetParam().crosapi_state == TestProfileParam::CrosapiParam::kEnabled) {
    GTEST_SKIP()
        << "Skipping test body for CrosapiParam::kEnabled, see b/287165243.";
  }

  if (profile_type() != TestProfileType::kRegular) {
    // Early return: DLP is only supported for regular profiles.
    return;
  }

  Profile* profile = browser()->profile();

  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      profile,
      base::BindRepeating(&FileTasksPolicyBrowserTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());

  ON_CALL(*rules_manager_, IsFilesPolicyEnabled)
      .WillByDefault(testing::Return(true));
  std::unique_ptr<policy::DlpFilesControllerAsh> files_controller_ =
      std::make_unique<policy::DlpFilesControllerAsh>(*rules_manager_);
  ON_CALL(*rules_manager_, GetDlpFilesController)
      .WillByDefault(testing::Return(files_controller_.get()));

  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(policy::DlpRulesManager::Level::kAllow));
  EXPECT_CALL(*rules_manager_,
              IsRestrictedDestination(GURL(blockedUrl), testing::_, testing::_,
                                      testing::_, testing::_, testing::_))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(policy::DlpRulesManager::Level::kBlock));

  std::vector<Expectation> expectations = {
      {"jpg/gif", kMediaAppId, nullptr, "https://example.com"},
      {"jpg/mp4", kMediaAppId, nullptr, "https://blocked.com"},
  };

  TestExpectationsAgainstDlp(expectations);
}

// |InProcessBrowserTest| which allows a fake user to login. Login a non-managed
// to ensure |IsEligibleAndEnabledUploadOfficeToCloud| returns the result of
// |IsUploadOfficeToCloudEnabled|.
class TestAccountBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  explicit TestAccountBrowserTest(TestAccountType test_account_type) {
    ash::LoggedInUserMixin::LogInType log_in_type =
        LogInTypeFor(test_account_type);
    absl::optional<AccountId> account_id = AccountIdFor(test_account_type);

    logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
        &mixin_host_, log_in_type, embedded_test_server(), this,
        /*should_launch_browser=*/true, account_id);
  }

  // Launch Files app and return its NativeWindow.
  gfx::NativeWindow LaunchFilesAppAndWait() {
    GURL files_swa_url = util::GetFileManagerMainPageUrlWithParams(
        ui::SelectFileDialog::SELECT_NONE, /*title=*/std::u16string(),
        /*current_directory_url=*/{},
        /*selection_url=*/GURL(),
        /*target_name=*/{}, /*file_types=*/{},
        /*file_type_index=*/0,
        /*search_query=*/{},
        /*show_android_picker_apps=*/false,
        /*volume_filter=*/{});
    ash::SystemAppLaunchParams params;
    params.url = files_swa_url;
    ash::LaunchSystemWebAppAsync(browser()->profile(),
                                 ash::SystemWebAppType::FILE_MANAGER, params);
    Browser* files_app = ui_test_utils::WaitForBrowserToOpen();
    return files_app->window()->GetNativeWindow();
  }

 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_->LogInUser();

    // Needed to launch Files app as the dialog's modal parent.
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
  }

 private:
  std::unique_ptr<ash::LoggedInUserMixin> logged_in_user_mixin_;
};

class NonManagedAccount : public TestAccountBrowserTest {
 public:
  NonManagedAccount() : TestAccountBrowserTest(kNonManaged) {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
  }

  void SetUpOnMainThread() override {
    TestAccountBrowserTest::SetUpOnMainThread();
    app_service_test_.SetUp(browser()->profile());
  }

  apps::AppServiceProxy* app_service_proxy() {
    apps::AppServiceProxy* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    CHECK(app_service_proxy);
    return app_service_proxy;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  apps::AppServiceTest app_service_test_;
};

// Tests that a |IsEligibleAndEnabledUploadOfficeToCloud| returns true when a
// non-managed user is logged in and |kUploadOfficeToCloud| is enabled.
IN_PROC_BROWSER_TEST_F(NonManagedAccount,
                       IsEligibleAndEnabledUploadOfficeToCloud) {
  ASSERT_TRUE(ash::cloud_upload::IsEligibleAndEnabledUploadOfficeToCloud(
      browser()->profile()));
}

// Test that the office PWA file handler is hidden from the available file
// handlers when opening an office file and the |kUploadOfficeToCloud| flag is
// enabled.
IN_PROC_BROWSER_TEST_F(NonManagedAccount, OfficePwaHandlerHidden) {
  struct FakeOfficeFileType {
    std::string file_extension;
    std::string mime_type;
  };

  std::vector<FakeOfficeFileType> fake_office_file_types = {
      {"ppt", "application/vnd.ms-powerpoint"},
      {"pptx",
       "application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation"},
      {"xls", "application/vnd.ms-excel"},
      {"xlsx",
       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
      {"doc", "application/msword"},
      {"docx",
       "application/"
       "vnd.openxmlformats-officedocument.wordprocessingml.document"}};

  for (FakeOfficeFileType& fake_office_file_type : fake_office_file_types) {
    file_manager::test::AddFakeWebApp(web_app::kMicrosoft365AppId,
                                      fake_office_file_type.mime_type,
                                      fake_office_file_type.file_extension,
                                      "something", true, app_service_proxy());

    base::FilePath test_file_path = web_app::CreateTestFileWithExtension(
        fake_office_file_type.file_extension);

    std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
        file_manager::test::GetTasksForFile(browser()->profile(),
                                            test_file_path);

    for (FullTaskDescriptor& task : tasks) {
      EXPECT_NE(web_app::kMicrosoft365AppId, task.task_descriptor.app_id)
          << " for extension: " << fake_office_file_type.file_extension;
    }
  }
}

class EnterpriseAccount : public TestAccountBrowserTest {
 public:
  EnterpriseAccount() : TestAccountBrowserTest(kEnterprise) {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a |IsEligibleAndEnabledUploadOfficeToCloud| returns false when an
// enterprise user is logged in and |kUploadOfficeToCloud| is enabled.
IN_PROC_BROWSER_TEST_F(EnterpriseAccount,
                       IsEligibleAndEnabledUploadOfficeToCloud) {
  ASSERT_FALSE(ash::cloud_upload::IsEligibleAndEnabledUploadOfficeToCloud(
      browser()->profile()));
}

class ChildAccount : public TestAccountBrowserTest {
 public:
  ChildAccount() : TestAccountBrowserTest(kChild) {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a |IsEligibleAndEnabledUploadOfficeToCloud| returns false when a
// child user is logged in and |kUploadOfficeToCloud| is enabled.
IN_PROC_BROWSER_TEST_F(ChildAccount, IsEligibleAndEnabledUploadOfficeToCloud) {
  ASSERT_FALSE(ash::cloud_upload::IsEligibleAndEnabledUploadOfficeToCloud(
      browser()->profile()));
}

class NonManagedAccountNoFlag : public TestAccountBrowserTest {
 public:
  NonManagedAccountNoFlag() : TestAccountBrowserTest(kNonManaged) {}
};

// Tests that a |IsEligibleAndEnabledUploadOfficeToCloud| returns false when a
// non-managed user is logged in but |kUploadOfficeToCloud| is disabled.
IN_PROC_BROWSER_TEST_F(NonManagedAccountNoFlag,
                       IsEligibleAndEnabledUploadOfficeToCloud) {
  ASSERT_FALSE(ash::cloud_upload::IsEligibleAndEnabledUploadOfficeToCloud(
      browser()->profile()));
}

// TODO(cassycc): move this class to a more appropriate spot.
// Fake DriveFs specific to the `DriveTest`. Allows a test file to
// be "added" to the DriveFs via `SetMetadata()`. The `alternate_url` of the
// file can be retrieved via `GetMetadata()`. This a simplified version of
// `FakeDriveFs` because the only condition for the file to be in the DriveFs is
// to have a `alternate_url_` entry.
class FakeSimpleDriveFs : public drivefs::FakeDriveFs {
 public:
  explicit FakeSimpleDriveFs(const base::FilePath& mount_path)
      : drivefs::FakeDriveFs(mount_path) {}

  // Sets `alternate_url_` which is retrieved later in `GetMetadata()`.
  void SetMetadata(const base::FilePath& path,
                   const std::string& alternate_url) {
    alternate_url_[path] = alternate_url;
  }

 private:
  // drivefs::mojom::DriveFs:
  // This is a simplified version of `FakeDriveFs::SetMetadata()` that just
  // returns a default `metadata` with the alternate_url set in `SetMetadata`.
  void GetMetadata(const base::FilePath& path,
                   GetMetadataCallback callback) override {
    auto metadata = drivefs::mojom::FileMetadata::New();
    metadata->alternate_url = alternate_url_[path];
    // Fill the rest of `metadata` with default values.
    metadata->content_mime_type = "";
    const drivefs::mojom::Capabilities& capabilities = {};
    metadata->capabilities = capabilities.Clone();
    metadata->folder_feature = {};
    metadata->available_offline = false;
    metadata->shared = false;
    std::move(callback).Run(drive::FILE_ERROR_OK, std::move(metadata));
  }

  // Each file in this DriveFs has an entry.
  std::unordered_map<base::FilePath, std::string> alternate_url_;
};

// TODO(cassycc): move this class to a more appropriate spot
// Fake DriveFs helper specific to the `DriveTest`. Implements the
// functions to create a `FakeSimpleDriveFs`.
class FakeSimpleDriveFsHelper : public drive::FakeDriveFsHelper {
 public:
  FakeSimpleDriveFsHelper(Profile* profile, const base::FilePath& mount_path)
      : drive::FakeDriveFsHelper(profile, mount_path),
        mount_path_(mount_path),
        fake_drivefs_(mount_path_) {}

  base::RepeatingCallback<std::unique_ptr<drivefs::DriveFsBootstrapListener>()>
  CreateFakeDriveFsListenerFactory() {
    return base::BindRepeating(&drivefs::FakeDriveFs::CreateMojoListener,
                               base::Unretained(&fake_drivefs_));
  }

  const base::FilePath& mount_path() { return mount_path_; }
  FakeSimpleDriveFs& fake_drivefs() { return fake_drivefs_; }

 private:
  const base::FilePath mount_path_;
  FakeSimpleDriveFs fake_drivefs_;
};

// TODO(cassycc or petermarshall) share this class with other test files for
// testing with a fake DriveFs.
// Tests the office fallback flow that occurs when
// a user fails to open an office file from Drive.
class DriveTest : public TestAccountBrowserTest {
 public:
  DriveTest() : TestAccountBrowserTest(kNonManaged) {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    drive_mount_point_ = temp_dir_.GetPath();
    test_file_name_ = "text.docx";
    // Path of test file relative to the DriveFs mount point.
    relative_test_file_path = base::FilePath("/").AppendASCII(test_file_name_);

    network_connection_tracker_ =
        network::TestNetworkConnectionTracker::CreateInstance();
  }

  DriveTest(const DriveTest&) = delete;
  DriveTest& operator=(const DriveTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    // Setup drive integration service.
    create_drive_integration_service_ = base::BindRepeating(
        &DriveTest::CreateDriveIntegrationService, base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  void TearDown() override {
    TestAccountBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    fake_drivefs_helpers_[profile] =
        std::make_unique<FakeSimpleDriveFsHelper>(profile, drive_mount_point_);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, "", drive_mount_point_,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

  Profile* profile() { return browser()->profile(); }

  mojo::Remote<drivefs::mojom::DriveFsDelegate>& drivefs_delegate() {
    return fake_drivefs_helpers_[profile()]->fake_drivefs().delegate();
  }

  base::FilePath observed_absolute_drive_path() {
    return base::FilePath(
        (drive_mount_point_.value() + relative_test_file_path.value()));
  }

  void SetNetwork(network::mojom::ConnectionType connection_type) {
    content::SetNetworkConnectionTrackerForTesting(nullptr);
    content::SetNetworkConnectionTrackerForTesting(
        network_connection_tracker_.get());
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  // Complete the set up of the fake DriveFs with a test file added.
  void SetUpTest() {
    // Install QuickOffice for the check in GetUserFallbackChoice() before
    // the office fallback dialog can launched.
    test::AddDefaultComponentExtensionsOnMainThread(profile());

    // Create Drive root directory.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::CreateDirectory(drive_mount_point_));
    }

    // Add test file to the DriveFs.
    fake_drivefs_helpers_[profile()]->fake_drivefs().SetMetadata(
        relative_test_file_path, alternate_url_);

    // Get URL for test file in the DriveFs.
    drive_test_file_url_ = ash::cloud_upload::FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        observed_absolute_drive_path());

    SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  }

 protected:
  const std::string alternate_url_ =
      "https://docs.google.com/document/d/smalldocxid?rtpof=true&usp=drive_fs";
  FileSystemURL drive_test_file_url_;

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath drive_mount_point_;
  std::string test_file_name_;
  base::FilePath relative_test_file_path;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  base::test::ScopedFeatureList feature_list_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<FakeSimpleDriveFsHelper>>
      fake_drivefs_helpers_;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Test to check that the test file fails to open when the system is offline but
// is successfully opened with a "try-again" dialog choice after the
// systems comes online.
IN_PROC_BROWSER_TEST_F(DriveTest, OfficeFallbackTryAgain) {
  // Add test file to fake DriveFs.
  SetUpTest();

  // Disable the setup flow for office files because we want the office
  // fallback dialog to run instead.
  SetWordFileHandlerToFilesSWA(profile(), kActionIdWebDriveOfficeWord);

  const TaskDescriptor web_drive_office_task = CreateWebDriveOfficeTask();
  std::vector<storage::FileSystemURL> file_urls{drive_test_file_url_};

  // Watch for dialog URL chrome://office-fallback.
  GURL expected_dialog_URL(chrome::kChromeUIOfficeFallbackURL);
  content::TestNavigationObserver navigation_observer_dialog(
      expected_dialog_URL);
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Fails as system is offline and thus will open office fallback dialog.
  ExecuteFileTask(
      profile(), web_drive_office_task, file_urls, nullptr,
      base::BindOnce(
          [](extensions::api::file_manager_private::TaskResult result,
             std::string error_message) {}));

  // Wait for office fallback dialog to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);

  // Start watching for the opening of `expected_web_drive_office_url`. The
  // query parameter is concatenated to the URL as office files opened from
  // drive have this query parameter added (https://crrev.com/c/3867338).
  GURL expected_web_drive_office_url(alternate_url_ + "&cros_files=true");
  content::TestNavigationObserver navigation_observer_office(
      expected_web_drive_office_url);
  navigation_observer_office.StartWatchingNewWebContents();

  // Run dialog callback, simulate user choosing to "try-again". Will succeed
  // because system is online.
  OnDialogChoiceReceived(profile(), web_drive_office_task, file_urls, nullptr,
                         ash::office_fallback::kDialogChoiceTryAgain);

  // Wait for file to open in web drive office.
  navigation_observer_office.Wait();
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Test that CloudOpenTask::Execute() will open a DriveFs office file when the
// cloud provider specified is Google Drive.
IN_PROC_BROWSER_TEST_F(DriveTest, OpenFileInDrive) {
  // Add test file to fake DriveFs.
  SetUpTest();

  std::vector<storage::FileSystemURL> file_urls{drive_test_file_url_};

  // Start watching for the opening of `expected_web_drive_office_url`. The
  // query parameter is concatenated to the URL as office files opened from
  // drive have this query parameter added (https://crrev.com/c/3867338).
  GURL expected_web_drive_office_url(alternate_url_ + "&cros_files=true");
  content::TestNavigationObserver navigation_observer_office(
      expected_web_drive_office_url);
  navigation_observer_office.StartWatchingNewWebContents();

  auto task = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
      profile(), file_urls, ash::cloud_upload::CloudProvider::kGoogleDrive,
      nullptr));
  task->OpenOrMoveFiles();

  // Wait for file to open in web drive office.
  navigation_observer_office.Wait();
}

// Test that the setup flow for office files, that has never been run before,
// will be run when a Web Drive Office task tries to open an office file
// already in DriveFs.
IN_PROC_BROWSER_TEST_F(DriveTest, FileInDriveOpensSetUpDialog) {
  // Add test file to fake DriveFs.
  SetUpTest();

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);

  // Create a Web Drive Office task to open the file from DriveFs. The file is
  // in the correct location for this task.
  const TaskDescriptor web_drive_office_task = CreateWebDriveOfficeTask();
  std::vector<storage::FileSystemURL> file_urls{drive_test_file_url_};

  // Watch for dialog URL chrome://cloud-upload.
  GURL expected_dialog_URL(chrome::kChromeUICloudUploadURL);
  content::TestNavigationObserver navigation_observer_dialog(
      expected_dialog_URL);
  navigation_observer_dialog.StartWatchingNewWebContents();

  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait();

  // Triggers setup flow.
  ExecuteFileTask(profile(), web_drive_office_task, file_urls, modal_parent,
                  base::DoNothing());

  // Wait for setup flow dialog to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());
}

// Test that the setup flow for office files, that has never been run before,
// will be run when a Web Drive Office task tries to open an office file not
// already in DriveFs.
IN_PROC_BROWSER_TEST_F(DriveTest, FileNotInDriveOpensSetUpDialog) {
  // Set up DriveFs.
  SetUpTest();

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);

  // Create a Web Drive Office task to open the file from DriveFs. The file is
  // not in the correct location for this task and would have to be moved to
  // DriveFs.
  const TaskDescriptor web_drive_office_task = CreateWebDriveOfficeTask();
  FileSystemURL file_outside_drive = CreateOfficeFileSourceURL(profile());
  std::vector<storage::FileSystemURL> file_urls{file_outside_drive};

  // Watch for dialog URL chrome://cloud-upload.
  GURL expected_dialog_URL(chrome::kChromeUICloudUploadURL);
  content::TestNavigationObserver navigation_observer_dialog(
      expected_dialog_URL);
  navigation_observer_dialog.StartWatchingNewWebContents();

  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait();

  // Triggers setup flow.
  ExecuteFileTask(
      profile(), web_drive_office_task, file_urls, modal_parent,
      base::BindOnce(
          [](extensions::api::file_manager_private::TaskResult result,
             std::string error_message) {}));

  // Wait for setup flow dialog to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());
}

// TODO(cassycc): move this class to a more appropriate spot
// Fake provided file system implementation specific to the `OneDriveTest`.
// Overrides the `GetActions` method so the `kOneDriveUrlActionId` and
// `kUserEmailActionId` actions are hardcoded to return for the test file.
class FakeProvidedFileSystemOneDrive
    : public ash::file_system_provider::FakeProvidedFileSystem {
 public:
  explicit FakeProvidedFileSystemOneDrive(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      const base::FilePath test_path_custom_actions)
      : FakeProvidedFileSystem(file_system_info),
        test_path_custom_actions_(test_path_custom_actions) {}

  ash::file_system_provider::AbortCallback GetActions(
      const std::vector<base::FilePath>& entry_paths,
      GetActionsCallback callback) override {
    ash::file_system_provider::Actions actions;
    for (auto& path : entry_paths) {
      if (path == test_path_custom_actions_) {
        actions.push_back(
            {ash::cloud_upload::kOneDriveUrlActionId, kODFSSampleUrl});
        actions.push_back(
            {ash::cloud_upload::kUserEmailActionId, kSampleUserEmail1});
        break;
      }
    }
    std::move(callback).Run(actions, base::File::FILE_OK);
    return ash::file_system_provider::AbortCallback();
  }

 private:
  const base::FilePath test_path_custom_actions_;
};

// TODO(cassycc): move this class to a more appropriate spot
// Fake extension provider specific to the `OneDriveTest`.
// Implements the functions to create a `FakeProvidedFileSystemOneDrive` with a
// test file added and passes along the appropriate `callback`.
class FakeExtensionProviderOneDrive
    : public ash::file_system_provider::FakeExtensionProvider {
 public:
  static std::unique_ptr<ProviderInterface> Create(
      const extensions::ExtensionId& extension_id,
      const base::FilePath test_path_within_odfs,
      std::string test_file_name) {
    ash::file_system_provider::Capabilities default_capabilities(
        false, false, false, extensions::SOURCE_NETWORK);
    return std::unique_ptr<ProviderInterface>(new FakeExtensionProviderOneDrive(
        extension_id, default_capabilities, test_path_within_odfs,
        test_file_name));
  }

  std::unique_ptr<ash::file_system_provider::ProvidedFileSystemInterface>
  CreateProvidedFileSystem(
      Profile* profile,
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info)
      override {
    DCHECK(profile);
    std::unique_ptr<FakeProvidedFileSystemOneDrive> fake_provided_file_system =
        std::make_unique<FakeProvidedFileSystemOneDrive>(
            file_system_info, test_path_within_odfs_);
    // Add test file.
    fake_provided_file_system->AddEntry(
        test_path_within_odfs_, false, test_file_name_, 0, base::Time::Now(),
        "application/"
        "vnd.openxmlformats-officedocument.wordprocessingml.document",
        "");
    return fake_provided_file_system;
  }

 private:
  FakeExtensionProviderOneDrive(
      const extensions::ExtensionId& extension_id,
      const ash::file_system_provider::Capabilities& capabilities,
      const base::FilePath test_path_within_odfs,
      std::string test_file_name)
      : FakeExtensionProvider(extension_id, capabilities),
        test_path_within_odfs_(test_path_within_odfs),
        test_file_name_(test_file_name) {}

  const base::FilePath test_path_within_odfs_;
  std::string test_file_name_;
};

// Fake app service web app publisher to test when an app is launched.
class FakeWebAppPublisher : public apps::AppPublisher {
 public:
  struct LaunchEvent {
    std::string app_id;
    std::string intent_url;
  };
  explicit FakeWebAppPublisher(Profile* profile)
      : AppPublisher(apps::AppServiceProxyFactory::GetForProfile(profile)) {
    RegisterPublisher(apps::AppType::kWeb);

    std::vector<apps::AppPtr> apps;
    auto ms_web_app = std::make_unique<apps::App>(apps::AppType::kWeb,
                                                  web_app::kMicrosoft365AppId);
    ms_web_app->readiness = apps::Readiness::kReady;
    apps.push_back(std::move(ms_web_app));
    Publish(std::move(apps), apps::AppType::kWeb,
            /*should_notify_initialized=*/true);
  }

  void LoadIcon(const std::string& app_id,
                const apps::IconKey& icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override {
    // Never called in these tests.
    NOTREACHED();
  }

  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::LaunchSource launch_source,
              apps::WindowInfoPtr window_info) override {
    // Never called in these tests.
    NOTREACHED();
  }

  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::IntentPtr intent,
                           apps::LaunchSource launch_source,
                           apps::WindowInfoPtr window_info,
                           apps::LaunchCallback callback) override {
    launches_.push_back({
        .app_id = app_id,
        .intent_url = (intent && intent->url) ? intent->url->spec() : "",
    });
  }

  void LaunchAppWithParams(apps::AppLaunchParams&& params,
                           apps::LaunchCallback callback) override {
    // Never called in these tests.
    NOTREACHED();
  }

  void ClearPastLaunches() { launches_.clear(); }

  std::vector<LaunchEvent> GetLaunches() { return launches_; }

 private:
  std::vector<LaunchEvent> launches_;
};

// TODO(cassycc or petermarshall) share this class with other test files for
// testing with a fake ODFS.
// Tests the office fallback flow that occurs when a
// user fails to open an office file from ODFS.
class OneDriveTest : public TestAccountBrowserTest {
 public:
  OneDriveTest() : TestAccountBrowserTest(kNonManaged) {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
    test_file_name_ = "text.docx";
    // Relative path for a file on ODFS and Android OneDrive.
    relative_test_path_ = base::FilePath(test_file_name_);
    // The path in ODFS is the relative path with "/" prefixed.
    test_path_within_odfs_ = base::FilePath("/").Append(relative_test_path_);
    file_system_id_ = "odfs";

    network_connection_tracker_ =
        network::TestNetworkConnectionTracker::CreateInstance();
  }

  OneDriveTest(const OneDriveTest&) = delete;
  OneDriveTest& operator=(const OneDriveTest&) = delete;

  void TearDown() override {
    TestAccountBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  // Creates and mounts fake provided file system for OneDrive with a test file
  // added. Installs QuickOffice for the check in GetUserFallbackChoice() before
  // the dialog can launched.
  void SetUpTest() {
    // Install QuickOffice for the check in GetUserFallbackChoice() before
    // the office fallback dialog can launched.
    test::AddDefaultComponentExtensionsOnMainThread(browser()->profile());

    service_ = ash::file_system_provider::Service::Get(profile());
    // Set `OneDriveTest::OpenWebAction` as the callback for the
    // `FakeProvidedFileSystemOneDrive`. The use of `base::Unretained()` is safe
    // because the class will exist for the duration of the test.
    service_->RegisterProvider(FakeExtensionProviderOneDrive::Create(
        extension_misc::kODFSExtensionId, test_path_within_odfs_,
        test_file_name_));
    provider_id_ = ash::file_system_provider::ProviderId::CreateFromExtensionId(
        extension_misc::kODFSExtensionId);
    ash::file_system_provider::MountOptions options(file_system_id_, "ODFS");
    EXPECT_EQ(base::File::FILE_OK,
              service_->MountFileSystem(provider_id_, options));

    // Get URL for test file in ODFS.
    odfs_test_file_url_ = ash::cloud_upload::FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        AbsoluteOdfsTestPath());

    web_app_publisher_ = std::make_unique<FakeWebAppPublisher>(profile());

    SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  }

  Profile* profile() { return browser()->profile(); }

  // The file path on ODFS which represents the fake file in OneDrive. This file
  // path can be used to open a file directly from ODFS using
  // `OpenOrMoveFiles()`.
  base::FilePath AbsoluteOdfsTestPath() {
    std::vector<ash::file_system_provider::ProvidedFileSystemInfo>
        file_systems = service_->GetProvidedFileSystemInfoList(provider_id_);
    // One and only one filesystem should be mounted for the ODFS extension.
    EXPECT_EQ(file_systems.size(), 1u);
    return file_systems[0].mount_path().Append(relative_test_path_);
  }

  // Filesystem path for Android OneDrive documents provider to the directory
  // that accesses the same OneDrive files as ODFS. The email is included in the
  // Root Document Id. The file path to a file is constructed by appending the
  // relative path (the part shared with ODFS). The file path on Android
  // OneDrive can be converted to a ODFS file path (with no email attached)
  // representing the same fake file in OneDrive. This file path can be used to
  // open a file indirectly (via ODFS) using `OpenOrMoveFiles()` if the
  // `user_email` matches the user email used in the
  // `FakeProvidedFileSystemOneDrive`.
  base::FilePath AndroidOneDrivePathToSharedDirectoryForEmail(
      std::string user_email) {
    // Filesystem path format is:
    // /mount_path/Files
    return AndroidOneDriveMountPathForEmail(user_email).Append("Files");
  }

  base::FilePath AndroidOneDriveMountPathForEmail(std::string user_email) {
    return arc::GetDocumentsProviderMountPath(
        "com.microsoft.skydrive.content.StorageAccessProvider",
        "pivots%2F" + user_email);
  }

  void SetNetwork(network::mojom::ConnectionType connection_type) {
    content::SetNetworkConnectionTrackerForTesting(nullptr);
    content::SetNetworkConnectionTrackerForTesting(
        network_connection_tracker_.get());
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

 protected:
  FileSystemURL odfs_test_file_url_;
  std::unique_ptr<FakeWebAppPublisher> web_app_publisher_;
  ash::file_system_provider::ProviderId provider_id_;
  base::FilePath relative_test_path_;
  base::FilePath test_path_within_odfs_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");

 private:
  base::test::ScopedFeatureList feature_list_;
  std::string file_system_id_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  raw_ptr<ash::file_system_provider::Service, ExperimentalAsh> service_;
  std::string test_file_name_;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Test to check that the test file fails to open when the system is offline but
// is successfully opened with a "try-again" dialog choice after the
// systems comes online.
IN_PROC_BROWSER_TEST_F(OneDriveTest, OfficeFallbackTryAgain) {
  // Creates a fake ODFS with a test file.
  SetUpTest();

  // Disable the setup flow for office files because we want the office
  // fallback dialog to run instead.
  SetWordFileHandlerToFilesSWA(profile(), kActionIdWebDriveOfficeWord);

  const TaskDescriptor open_in_office_task = CreateOpenInOfficeTask();
  std::vector<storage::FileSystemURL> file_urls{odfs_test_file_url_};

  // Watch for dialog URL chrome://office-fallback.
  GURL expected_dialog_URL(chrome::kChromeUIOfficeFallbackURL);
  content::TestNavigationObserver navigation_observer_dialog(
      expected_dialog_URL);
  navigation_observer_dialog.StartWatchingNewWebContents();

  web_app_publisher_->ClearPastLaunches();

  // Fails as system is offline and thus will open office fallback dialog.
  ExecuteFileTask(
      profile(), open_in_office_task, file_urls, nullptr,
      base::BindOnce(
          [](extensions::api::file_manager_private::TaskResult result,
             std::string error_message) {}));

  // Wait for office fallback dialog to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  CHECK_EQ(0u, web_app_publisher_->GetLaunches().size());

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);

  // Run dialog callback, simulate user choosing to "try-again". Will succeed
  // because system is online, and the file doesn't need to be moved.
  OnDialogChoiceReceived(profile(), open_in_office_task, file_urls, nullptr,
                         ash::office_fallback::kDialogChoiceTryAgain);

  auto launches = web_app_publisher_->GetLaunches();
  ASSERT_EQ(1u, launches.size());
  CHECK_EQ(launches[0].app_id, web_app::kMicrosoft365AppId);
  CHECK_EQ(launches[0].intent_url, kODFSSampleUrl);
}

// Test to check that the test file fails to open when the system is offline and
// does not open from a "cancel" dialog choice even when the systems comes
// online.
IN_PROC_BROWSER_TEST_F(OneDriveTest, OfficeFallbackCancel) {
  // Creates a fake ODFS with a test file.
  SetUpTest();

  // Disable the setup flow for office files because we want the office
  // fallback dialog to run instead.
  SetWordFileHandlerToFilesSWA(profile(), kActionIdWebDriveOfficeWord);

  const TaskDescriptor open_in_office_task = CreateOpenInOfficeTask();
  std::vector<storage::FileSystemURL> file_urls{odfs_test_file_url_};

  // Watch for dialog URL chrome://office-fallback.
  GURL expected_dialog_URL(chrome::kChromeUIOfficeFallbackURL);
  content::TestNavigationObserver navigation_observer_dialog(
      expected_dialog_URL);
  navigation_observer_dialog.StartWatchingNewWebContents();

  web_app_publisher_->ClearPastLaunches();

  // Fails as system is offline and thus will open office fallback dialog.
  ExecuteFileTask(
      profile(), open_in_office_task, file_urls, nullptr,
      base::BindOnce(
          [](extensions::api::file_manager_private::TaskResult result,
             std::string error_message) {}));

  // Wait for office fallback dialog to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  ASSERT_EQ(0u, web_app_publisher_->GetLaunches().size());

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);

  // Run dialog callback, simulate user choosing to "cancel". The file will not
  // open.
  OnDialogChoiceReceived(profile(), open_in_office_task, file_urls, nullptr,
                         ash::office_fallback::kDialogChoiceCancel);

  ASSERT_EQ(0u, web_app_publisher_->GetLaunches().size());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Test that OpenOrMoveFiles() will open a ODFS office file when the cloud
// provider specified is OneDrive.
IN_PROC_BROWSER_TEST_F(OneDriveTest, OpenFileFromODFS) {
  // Creates a fake ODFS with a test file.
  SetUpTest();

  std::vector<storage::FileSystemURL> file_urls{odfs_test_file_url_};

  web_app_publisher_->ClearPastLaunches();

  // Open file directly from ODFS.
  auto task = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
      profile(), file_urls, ash::cloud_upload::CloudProvider::kOneDrive,
      nullptr));
  task->OpenOrMoveFiles();

  auto launches = web_app_publisher_->GetLaunches();
  ASSERT_EQ(1u, launches.size());
  CHECK_EQ(launches[0].app_id, web_app::kMicrosoft365AppId);
  CHECK_EQ(launches[0].intent_url, kODFSSampleUrl);
}

// Test that OpenOrMoveFiles() will open the Move Confirmation dialog when the
// cloud provider specified is OneDrive but the office file to be opened needs
// to be moved to ODFS.
IN_PROC_BROWSER_TEST_F(OneDriveTest, OpenFileNotFromODFS) {
  FileSystemURL file_outside_one_drive = CreateOfficeFileSourceURL(profile());
  std::vector<storage::FileSystemURL> file_urls{file_outside_one_drive};

  // Watch for dialog URL chrome://cloud-upload.
  GURL expected_dialog_URL(chrome::kChromeUICloudUploadURL);
  content::TestNavigationObserver navigation_observer_dialog(
      expected_dialog_URL);
  navigation_observer_dialog.StartWatchingNewWebContents();

  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait();

  // Triggers Move Confirmation dialog.
  auto task = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
      profile(), file_urls, ash::cloud_upload::CloudProvider::kOneDrive,
      modal_parent));
  task->OpenOrMoveFiles();

  // Wait for setup flow dialog to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());
}

// Test that OpenOrMoveFiles() will open an Android OneDrive office file via
// ODFS when the cloud provider specified is OneDrive. Test that the file path
// is not on ODFS but does get opened via ODFS.
IN_PROC_BROWSER_TEST_F(OneDriveTest, OpenFileFromAndroidOneDriveViaODFS) {
  // Create a fake ODFS with a test file.
  SetUpTest();

  // Create equivalent file path in Android OneDrive with same email (email
  // matches `FakeProvidedFileSystemOneDrive`).
  base::FilePath android_onedrive_path_same_email =
      AndroidOneDrivePathToSharedDirectoryForEmail(kSampleUserEmail1)
          .Append(relative_test_path_);

  // Create a FileSystemURL from the Android OneDrive file path.
  FileSystemURL android_onedrive_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeArcDocumentsProvider,
      android_onedrive_path_same_email);

  web_app_publisher_->ClearPastLaunches();

  // Open the file indirectly from Android OneDrive (via ODFS).
  auto task = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
      profile(), {android_onedrive_url},
      ash::cloud_upload::CloudProvider::kOneDrive, nullptr));
  task->OpenOrMoveFiles();

  auto launches = web_app_publisher_->GetLaunches();
  ASSERT_EQ(1u, launches.size());
  CHECK_EQ(launches[0].app_id, web_app::kMicrosoft365AppId);
  // Check that the ODFS URL was opened.
  CHECK_EQ(launches[0].intent_url, kODFSSampleUrl);
}

// Test that OpenOrMoveFiles() will not open an Android OneDrive office file via
// ODFS when the cloud provider specified is OneDrive but the email accounts for
// ODFS and Android OneDrive don't match. The Android OneDrive file path will
// convert to a valid ODFS file path but as the email accounts don't match, the
// file won't open.
IN_PROC_BROWSER_TEST_F(OneDriveTest,
                       FailToOpenFileFromAndroidOneDriveViaODFSDiffEmail) {
  // Create a fake ODFS with a test file.
  SetUpTest();

  // Create equivalent file path in Android OneDrive with different email
  // (email doesn't match `FakeProvidedFileSystemOneDrive`).
  base::FilePath android_onedrive_path_diff_email =
      AndroidOneDrivePathToSharedDirectoryForEmail(kSampleUserEmail2)
          .Append(relative_test_path_);

  // Create a FileSystemURL from the Android OneDrive file path.
  FileSystemURL android_onedrive_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeArcDocumentsProvider,
      android_onedrive_path_diff_email);

  web_app_publisher_->ClearPastLaunches();

  // Attempt to open the file indirectly from Android OneDrive (via ODFS). It
  // will fail as the email accounts don't match.
  auto task = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
      profile(), {android_onedrive_url},
      ash::cloud_upload::CloudProvider::kOneDrive, nullptr));
  task->OpenOrMoveFiles();

  auto launches = web_app_publisher_->GetLaunches();
  ASSERT_EQ(0u, launches.size());
}

// Test that OpenOrMoveFiles() will not open an Android OneDrive office file
// (with the expected Url format) via ODFS when the cloud provider specified is
// OneDrive but the Android OneDrive path does not exist on ODFS.
IN_PROC_BROWSER_TEST_F(OneDriveTest,
                       FailToOpenFileFromAndroidOneDriveNotOnODFS) {
  // Create a fake ODFS with a test file.
  SetUpTest();

  // Create file path in Android OneDrive that is in the "Files" directory but
  // doesn't exist on ODFS.
  base::FilePath android_onedrive_path_no_equivalent =
      AndroidOneDrivePathToSharedDirectoryForEmail(kSampleUserEmail1)
          .Append("another_file.docx");

  // Create a FileSystemURL from the Android OneDrive file path.
  FileSystemURL android_onedrive_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeArcDocumentsProvider,
      android_onedrive_path_no_equivalent);

  web_app_publisher_->ClearPastLaunches();

  // Attempt to open the file indirectly from Android OneDrive (via ODFS). It
  // will fail as there is not an equivalent ODFS file path.
  auto task = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
      profile(), {android_onedrive_url},
      ash::cloud_upload::CloudProvider::kOneDrive, nullptr));
  task->OpenOrMoveFiles();

  auto launches = web_app_publisher_->GetLaunches();
  ASSERT_EQ(0u, launches.size());
}

// Test that OpenOrMoveFiles() will not open an Android OneDrive office file
// (that doesn't have the expected Url format) via ODFS when the cloud provider
// specified is OneDrive but the Android OneDrive path does not have an
// equivalent on ODFS.
IN_PROC_BROWSER_TEST_F(
    OneDriveTest,
    FailToOpenFileFromAndroidOneDriveDirectoryNotAccessibleToODFS) {
  // Create a fake ODFS with a test file.
  SetUpTest();

  // Create file path in Android OneDrive for a file that is not accessible to
  // ODFS. For example, the "Files" folder is accessible to both file systems,
  // but the "Shared" folder is only accessible to Android OneDrive.
  base::FilePath android_onedrive_path_no_equivalent =
      AndroidOneDriveMountPathForEmail(kSampleUserEmail1)
          .Append("Shared")
          .Append(relative_test_path_);

  // Create a FileSystemURL from the Android OneDrive file path.
  FileSystemURL android_onedrive_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeArcDocumentsProvider,
      android_onedrive_path_no_equivalent);

  web_app_publisher_->ClearPastLaunches();

  // Attempt to open the file indirectly from Android OneDrive (via ODFS). It
  // will fail as there is not an equivalent ODFS file path.
  auto task = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
      profile(), {android_onedrive_url},
      ash::cloud_upload::CloudProvider::kOneDrive, nullptr));
  task->OpenOrMoveFiles();

  auto launches = web_app_publisher_->GetLaunches();
  ASSERT_EQ(0u, launches.size());
}

// Test that the setup flow for office files, that has never been run before,
// will be run when an Open in Office task tries to open an office file
// already in ODFS.
IN_PROC_BROWSER_TEST_F(OneDriveTest, FileInOneDriveOpensSetUpDialog) {
  // Do this before SetUpTest creates a FakeWebAppPublisher which would
  // intercept Files app launching.
  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait();

  // Creates a fake ODFS with a test file.
  SetUpTest();

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);

  // Create an Open in Office task to open the file from ODFS. The file is in
  // the correct location for this task.
  const TaskDescriptor open_in_office_task = CreateOpenInOfficeTask();
  std::vector<storage::FileSystemURL> file_urls{odfs_test_file_url_};

  // Watch for dialog URL chrome://cloud-upload.
  GURL expected_dialog_URL(chrome::kChromeUICloudUploadURL);
  content::TestNavigationObserver navigation_observer_dialog(
      expected_dialog_URL);
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Triggers setup flow.
  ExecuteFileTask(profile(), open_in_office_task, file_urls, modal_parent,
                  base::DoNothing());

  // Wait for setup flow dialog to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());
}

// Test that the setup flow for office files, that has never been run before,
// will be run when an Open in Office task tries to open an office file not
// already in ODFS.
IN_PROC_BROWSER_TEST_F(OneDriveTest, FileNotInOneDriveOpensSetUpDialog) {
  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);

  // Create an Open in Office task to open the file from ODFS. The file is not
  // in the correct location for this task and would have to be moved to ODFS.
  const TaskDescriptor open_in_office_task = CreateOpenInOfficeTask();
  FileSystemURL file_outside_one_drive = CreateOfficeFileSourceURL(profile());
  std::vector<storage::FileSystemURL> file_urls{file_outside_one_drive};

  // Watch for dialog URL chrome://cloud-upload.
  GURL expected_dialog_URL(chrome::kChromeUICloudUploadURL);
  content::TestNavigationObserver navigation_observer_dialog(
      expected_dialog_URL);
  navigation_observer_dialog.StartWatchingNewWebContents();

  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait();

  // Triggers setup flow.
  ExecuteFileTask(
      profile(), open_in_office_task, file_urls, modal_parent,
      base::BindOnce(
          [](extensions::api::file_manager_private::TaskResult result,
             std::string error_message) {}));

  // Wait for setup flow dialog to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    FileTasksBrowserTest);
INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    FileTasksPolicyBrowserTest);

}  // namespace file_tasks
}  // namespace file_manager
