// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/web_applications/default_web_app_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/features.h"

using chromeos::default_web_apps::kMediaAppId;

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

  bool has_media_app = false;
  bool has_gallery = false;
  for (const auto& t : *result) {
    has_media_app = has_media_app || t.task_descriptor().app_id == kMediaAppId;
    has_gallery = has_gallery || t.task_descriptor().app_id == kGalleryAppId;
  }
  // Gallery and Media App should never both appear as task options.
  EXPECT_TRUE(!(has_media_app && has_gallery)) << expectation.file_extensions;

  auto default_task =
      std::find_if(result->begin(), result->end(),
                   [](const auto& task) { return task.is_default(); });

  // Early exit for the uncommon situation where no default should be set.
  if (!expectation.app_id) {
    EXPECT_TRUE(default_task == result->end()) << expectation.file_extensions;
    return;
  }

  ASSERT_TRUE(default_task != result->end()) << expectation.file_extensions;

  EXPECT_EQ(expectation.app_id, default_task->task_descriptor().app_id)
      << " for extension: " << expectation.file_extensions;

  // Verify no other task is set as default.
  EXPECT_EQ(1,
            std::count_if(result->begin(), result->end(),
                          [](const auto& task) { return task.is_default(); }))
      << expectation.file_extensions;
}

// Helper to quit a run loop after invoking VerifyTasks().
void VerifyAsyncTask(int* remaining,
                     Expectation expectation,
                     const base::Closure& quit_closure,
                     std::unique_ptr<std::vector<FullTaskDescriptor>> result) {
  VerifyTasks(remaining, expectation, std::move(result));
  quit_closure.Run();
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
    web_app::WebAppProvider::Get(browser()->profile())
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
      std::vector<base::StringPiece> all_extensions =
          base::SplitStringPiece(test.file_extensions, "/",
                                 base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      for (base::StringPiece extension : all_extensions) {
        base::FilePath path = prefix.AddExtension(extension);
        std::string mime_type;
        net::GetMimeTypeFromFile(path, &mime_type);
        if (test.mime_type) {
          // Sniffing isn't used when GetMimeTypeFromFile() succeeds, so there
          // shouldn't be a hard-coded mime type configured.
          EXPECT_TRUE(mime_type.empty());
          mime_type = test.mime_type;
        }
        EXPECT_FALSE(mime_type.empty()) << "No mime type for " << path;
        entries.push_back({path, mime_type, false});
      }
      std::vector<GURL> file_urls{entries.size(), GURL()};

      // task_verifier callback is invoked synchronously from
      // FindAllTypesOfTasks.
      FindAllTypesOfTasks(browser()->profile(), entries, file_urls,
                          base::BindOnce(&VerifyTasks, &remaining, test));
    }
    EXPECT_EQ(0, remaining);
  }
};

class FileTasksBrowserTest : public FileTasksBrowserTestBase {
 public:
  FileTasksBrowserTest() {
    // Disable Media App.
    scoped_feature_list_.InitWithFeatures({}, {chromeos::features::kMediaApp});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FileTasksBrowserTestWithMediaApp : public FileTasksBrowserTestBase {
 public:
  FileTasksBrowserTestWithMediaApp() {
    // Enable Media App.
    scoped_feature_list_.InitWithFeatures({chromeos::features::kMediaApp}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// List of single file default app expectations that we don't expect to change
// regardless of app flags. Changes to this test may have implications for file
// handling declarations in built-in app manifests, because logic in
// ChooseAndSetDefaultTask() treats handlers for extensions with a higher
// priority than handlers for mime types.
// Provide MIME types here for extensions known to be missing mime types from
// net::GetMimeTypeFromFile() (see ExtensionToMimeMapping test). In practice,
// these MIME types are populated via file sniffing, but tests in this file do
// not operate on real files. We hard code MIME types that file sniffing
// obtained experimentally from sample files.
constexpr Expectation kUnchangedExpectations[] = {
    // Raw.
    {"arw", kGalleryAppId, "image/tiff"},
    {"cr2", kGalleryAppId, "image/tiff"},
    {"dng", kGalleryAppId, "image/tiff"},
    {"nef", kGalleryAppId, "image/tiff"},
    {"nrw", kGalleryAppId, "image/tiff"},
    {"orf", kGalleryAppId, "image/tiff"},
    {"raf", kGalleryAppId, "image/tiff"},
    {"rw2", kGalleryAppId, "image/tiff"},
    {"NRW", kGalleryAppId, "image/tiff"},  // Uppercase extension.

    // Video.
    {"3gp", kVideoPlayerAppId, "application/octet-stream"},
    {"avi", kVideoPlayerAppId, "application/octet-stream"},
    {"m4v", kVideoPlayerAppId},
    {"mkv", kVideoPlayerAppId, "video/webm"},
    {"mov", kVideoPlayerAppId, "application/octet-stream"},
    {"mp4", kVideoPlayerAppId},
    {"mpeg", kVideoPlayerAppId},
    {"mpeg4", kVideoPlayerAppId, "video/mpeg"},
    {"mpg", kVideoPlayerAppId},
    {"mpg4", kVideoPlayerAppId, "video/mpeg"},
    {"ogm", kVideoPlayerAppId},
    {"ogv", kVideoPlayerAppId},
    {"ogx", kVideoPlayerAppId, "video/ogg"},
    {"webm", kVideoPlayerAppId},

    // Audio.
    {"amr", kAudioPlayerAppId, "application/octet-stream"},
    {"flac", kAudioPlayerAppId},
    {"m4a", kAudioPlayerAppId},
    {"mp3", kAudioPlayerAppId},
    {"oga", kAudioPlayerAppId},
    {"ogg", kAudioPlayerAppId},
    {"wav", kAudioPlayerAppId},
};

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
  //  With the Media App disabled, all images should be handled by Gallery.
  std::vector<Expectation> expectations = {
      // Images.
      {"bmp", kGalleryAppId},  {"gif", kGalleryAppId},  {"ico", kGalleryAppId},
      {"jpg", kGalleryAppId},  {"jpeg", kGalleryAppId}, {"png", kGalleryAppId},
      {"webp", kGalleryAppId},
  };
  expectations.insert(expectations.end(), std::begin(kUnchangedExpectations),
                      std::end(kUnchangedExpectations));

  TestExpectationsAgainstDefaultTasks(expectations);
}

// Spot test the default handlers for selections that include multiple different
// file types. Only tests combinations of interest to the Media App.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, MultiSelectDefaultHandler) {
  std::vector<Expectation> expectations = {
      {"jpg/gif", kGalleryAppId},
      {"jpg/mp4", kGalleryAppId},
  };

  TestExpectationsAgainstDefaultTasks(expectations);
}

// Tests the default handlers with the Media App installed.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTestWithMediaApp,
                       DefaultHandlerChangeDetector) {
  // With the Media App enabled, images should be handled by it by default (but
  // video, which it also handles should be unchanged).
  std::vector<Expectation> expectations = {
      // Images.
      {"bmp", kMediaAppId},  {"gif", kMediaAppId},  {"ico", kMediaAppId},
      {"jpg", kMediaAppId},  {"jpeg", kMediaAppId}, {"png", kMediaAppId},
      {"webp", kMediaAppId},
  };
  expectations.insert(expectations.end(), std::begin(kUnchangedExpectations),
                      std::end(kUnchangedExpectations));

  TestExpectationsAgainstDefaultTasks(expectations);
}

// Spot test the default handlers for selections that include multiple different
// file types with the Media App installed.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTestWithMediaApp,
                       MultiSelectDefaultHandler) {
  std::vector<Expectation> expectations = {
      {"jpg/gif", kMediaAppId},
      // Test video specifically since the Media App's manifest specifies it
      // handles video files. Note Gallery was never intended to handle video,
      // (and the video app can never handle images) so, until video support
      // has more polish in the Media App, this is the only case where no app
      // is given a clear preference. Media App will be present, but won't be
      // marked default because it is not marked as an "extension match" nor is
      // it a fallback handler.
      {"jpg/mp4", nullptr},
  };

  TestExpectationsAgainstDefaultTasks(expectations);
}

// Sanity check: the tiff-specific file handler is preferred when MediaApp is
// not enabled.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTest, InstalledAppsAreImplicitDefaults) {
  if (GetParam() == TestProfileType::kRegular) {
    auto extension = InstallTiffHandlerChromeApp(browser()->profile());
    TestExpectationsAgainstDefaultTasks({{"tiff", extension->id().c_str()}});
  } else if (GetParam() == TestProfileType::kGuest) {
    // The extension can't be installed in guest mode. (FATAL check under
    // ExtensionService::AddNewOrUpdatedExtension()). There should be no tiff
    // handler.
    TestExpectationsAgainstDefaultTasks({{"tiff", nullptr}});
  } else {
    // The extension installs in incognito, but doesn't register a file handler.
    // So, also no tiff handler.
    InstallTiffHandlerChromeApp(browser()->profile());
    TestExpectationsAgainstDefaultTasks({{"tiff", nullptr}});
  }
}

// If the media app is enabled, it will be preferred over a chrome app with a
// specific extension, unless that app is set default via prefs.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTestWithMediaApp,
                       MediaAppPreferredOverChromeApps) {
  if (GetParam() == TestProfileType::kGuest) {
    // The provided file system can't install in guest mode. Just check that
    // MediaApp handles tiff.
    TestExpectationsAgainstDefaultTasks({{"tiff", kMediaAppId}});
    return;
  }
  Profile* profile = browser()->profile();
  auto extension = InstallTiffHandlerChromeApp(profile);
  TestExpectationsAgainstDefaultTasks({{"tiff", kMediaAppId}});

  UpdateDefaultTask(profile->GetPrefs(), extension->id() + "|app|tiffAction",
                    {"tiff"}, {"image/tiff"});
  if (GetParam() == TestProfileType::kIncognito) {
    // In incognito, the provided file system can exist, but the file handler
    // preference can't be changed.
    TestExpectationsAgainstDefaultTasks({{"tiff", kMediaAppId}});
  } else {
    TestExpectationsAgainstDefaultTasks({{"tiff", extension->id().c_str()}});
  }
}

// Test expectations for files coming from provided file systems.
IN_PROC_BROWSER_TEST_P(FileTasksBrowserTestWithMediaApp,
                       ProvidedFileSystemFileSource) {
  if (GetParam() == TestProfileType::kGuest) {
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
      profile, volume->mount_path().AppendASCII(kTestFile), kFileManagerAppId,
      &url));

  // Note |url| differs slightly to the result of ToGURL() below. The colons
  // either side of `:test-image-provider-fs:` become escaped as `%3A`.

  storage::FileSystemURL filesystem_url =
      util::GetFileSystemContextForExtensionId(profile, kFileManagerAppId)
          ->CrackURL(url);

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

INSTANTIATE_TEST_SUITE_P(All,
                         FileTasksBrowserTest,
                         ::testing::Values(TestProfileType::kRegular,
                                           TestProfileType::kIncognito,
                                           TestProfileType::kGuest),
                         TestProfileTypeToString);

INSTANTIATE_TEST_SUITE_P(All,
                         FileTasksBrowserTestWithMediaApp,
                         ::testing::Values(TestProfileType::kRegular,
                                           TestProfileType::kIncognito,
                                           TestProfileType::kGuest),
                         TestProfileTypeToString);

}  // namespace file_tasks
}  // namespace file_manager
