// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MediaGalleries gallery watch API browser tests.

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

// Id of test extension from
// chrome/test/data/extensions/api_test/|kTestExtensionPath|
const char kTestExtensionId[] = "gceegfkgibmgpfopknlcgleimclbknie";
const char kTestExtensionPath[] = "media_galleries/gallerywatch";

// JS commands.
const char kGetMediaFileSystemsCmd[] = "getMediaFileSystems()";
const char kSetupWatchOnValidGalleriesCmd[] = "setupWatchOnValidGalleries()";
const char kSetupWatchOnUnlistenedValidGalleriesCmd[] =
    "setupWatchOnUnlistenedValidGalleries()";
const char kAddGalleryChangedListenerCmd[] = "addGalleryChangedListener()";
const char kAddCheckingGalleryChangedListenerCmd[] =
    "addCheckingGalleryChangedListener()";
const char kRemoveGalleryChangedListenerCmd[] =
    "removeGalleryChangedListener()";
const char kRemoveGalleryWatchCmd[] = "removeGalleryWatch()";
const char kSetupWatchOnInvalidGalleryCmd[] = "setupWatchOnInvalidGallery()";

// And JS reply messages.
const char kAddGalleryWatchOK[] = "add_gallery_watch_ok";
const char kGetMediaFileSystemsCallbackOK[] =
    "get_media_file_systems_callback_ok";
const char kGetMediaFileSystemsOK[] = "get_media_file_systems_ok";
const char kAddGalleryChangedListenerOK[] = "add_gallery_changed_listener_ok";
const char kRemoveGalleryChangedListenerOK[] =
    "remove_gallery_changed_listener_ok";
const char kRemoveGalleryWatchOK[] = "remove_gallery_watch_ok";
const char kOnGalleryChangedCheckingOK[] = "on_gallery_changed_checking_ok";

// Test reply messages.
const char kAddGalleryWatchRequestFailed[] = "add_watch_request_failed";
const char kAddGalleryWatchRequestRuntimeError[] =
    "add_watch_request_runtime_error";
const char kAddGalleryWatchRequestSucceeded[] = "add_watch_request_succeeded";
const char kGalleryChangedEventReceived[] = "gallery_changed_event_received";

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//                 MediaGalleriesGalleryWatchApiTest                         //
///////////////////////////////////////////////////////////////////////////////

class MediaGalleriesGalleryWatchApiTest : public extensions::ExtensionApiTest {
 public:
  MediaGalleriesGalleryWatchApiTest()
      : extension_(NULL), background_host_(NULL) {}
  ~MediaGalleriesGalleryWatchApiTest() override {}

 protected:
  // ExtensionApiTest overrides.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID, kTestExtensionId);
  }
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    ensure_media_directories_exists_.reset(new EnsureMediaDirectoriesExists);
    extension_ = LoadExtension(test_data_dir_.AppendASCII(kTestExtensionPath));
    GetBackgroundHostForTestExtension();
    CreateTestGallery();
    FetchMediaGalleriesList();
  }
  void TearDownOnMainThread() override {
    extension_ = NULL;
    background_host_ = NULL;
    ensure_media_directories_exists_.reset();
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  bool GalleryWatchesSupported() {
    return base::FilePathWatcher::RecursiveWatchAvailable();
  }

  void ExecuteCmdAndCheckReply(const std::string& js_command,
                               const std::string& ok_message) {
    ExtensionTestMessageListener listener(ok_message, false);
    background_host_->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(js_command), base::NullCallback());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  bool AddNewFileInTestGallery() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath gallery_file =
        test_gallery_.GetPath().Append(FILE_PATH_LITERAL("test1.txt"));
    std::string content("new content");
    int write_size =
        base::WriteFile(gallery_file, content.c_str(), content.length());
    return (write_size == static_cast<int>(content.length()));
  }

  void SetupGalleryWatches() {
    std::string expected_result = GalleryWatchesSupported()
                                      ? kAddGalleryWatchRequestSucceeded
                                      : kAddGalleryWatchRequestFailed;

    ExtensionTestMessageListener add_gallery_watch_finished(
        expected_result, false /* no reply */);
    ExecuteCmdAndCheckReply(kSetupWatchOnValidGalleriesCmd, kAddGalleryWatchOK);
    EXPECT_TRUE(add_gallery_watch_finished.WaitUntilSatisfied());
  }

 private:
  void GetBackgroundHostForTestExtension() {
    ASSERT_TRUE(extension_);
    background_host_ = extensions::ProcessManager::Get(browser()->profile())
                           ->GetBackgroundHostForExtension(extension_->id())
                           ->render_view_host();
    ASSERT_TRUE(background_host_);
  }

  void CreateTestGallery() {
    MediaGalleriesPreferences* preferences =
        g_browser_process->media_file_system_registry()->GetPreferences(
            browser()->profile());
    base::RunLoop runloop;
    preferences->EnsureInitialized(runloop.QuitClosure());
    runloop.Run();

    ASSERT_TRUE(test_gallery_.CreateUniqueTempDir());
    MediaGalleryPrefInfo gallery_info;
    ASSERT_FALSE(preferences->LookUpGalleryByPath(test_gallery_.GetPath(),
                                                  &gallery_info));
    MediaGalleryPrefId id = preferences->AddGallery(
        gallery_info.device_id, gallery_info.path,
        MediaGalleryPrefInfo::kAutoDetected, gallery_info.volume_label,
        gallery_info.vendor_name, gallery_info.model_name,
        gallery_info.total_size_in_bytes, gallery_info.last_attach_time, 0, 0,
        0);

    preferences->SetGalleryPermissionForExtension(*extension_, id, true);
  }

  void FetchMediaGalleriesList() {
    ExtensionTestMessageListener get_media_systems_finished(
        kGetMediaFileSystemsCallbackOK, false /* no reply */);
    ExecuteCmdAndCheckReply(kGetMediaFileSystemsCmd, kGetMediaFileSystemsOK);
    EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());
  }

  std::unique_ptr<EnsureMediaDirectoriesExists>
      ensure_media_directories_exists_;

  base::ScopedTempDir test_gallery_;

  const extensions::Extension* extension_;

  content::RenderViewHost* background_host_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesGalleryWatchApiTest);
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesGalleryWatchApiTest, BasicGalleryWatch) {
  // Add gallery watch listener.
  ExecuteCmdAndCheckReply(kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);

  SetupGalleryWatches();

  // Modify gallery contents.
  ExtensionTestMessageListener gallery_change_event_received(
      kGalleryChangedEventReceived, false /* no reply */);

  ASSERT_TRUE(AddNewFileInTestGallery());
  if (GalleryWatchesSupported())
    EXPECT_TRUE(gallery_change_event_received.WaitUntilSatisfied());

  // Remove gallery watch listener.
  ExecuteCmdAndCheckReply(kRemoveGalleryChangedListenerCmd,
                          kRemoveGalleryChangedListenerOK);

  // Remove gallery watch request.
  if (GalleryWatchesSupported())
    ExecuteCmdAndCheckReply(kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesGalleryWatchApiTest,
                       CorrectResponseOnModifyingWatchedGallery) {
  if (!GalleryWatchesSupported())
    return;

  // Add gallery watch listener.
  ExecuteCmdAndCheckReply(kAddCheckingGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);
  SetupGalleryWatches();

  // Modify gallery contents; expect correct details.
  ExtensionTestMessageListener got_correct_details(kOnGalleryChangedCheckingOK,
                                                   false);
  ASSERT_TRUE(AddNewFileInTestGallery());
  EXPECT_TRUE(got_correct_details.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesGalleryWatchApiTest,
                       RemoveListenerAndModifyGallery) {
  if (!GalleryWatchesSupported())
    return;

  // Add a gallery watch listener.
  ExecuteCmdAndCheckReply(kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);
  SetupGalleryWatches();

  // Modify gallery contents.
  ExtensionTestMessageListener gallery_change_event_received(
      kGalleryChangedEventReceived, false /* no reply */);
  ASSERT_TRUE(AddNewFileInTestGallery());
  EXPECT_TRUE(gallery_change_event_received.WaitUntilSatisfied());

  // Remove gallery watch listener.
  ExecuteCmdAndCheckReply(kRemoveGalleryChangedListenerCmd,
                          kRemoveGalleryChangedListenerOK);

  // No listener, modify gallery contents.
  ASSERT_TRUE(AddNewFileInTestGallery());

  // Remove gallery watch.
  ExecuteCmdAndCheckReply(kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesGalleryWatchApiTest,
                       SetupGalleryWatchWithoutListeners) {
  if (!GalleryWatchesSupported())
    return;

  ExecuteCmdAndCheckReply(kSetupWatchOnUnlistenedValidGalleriesCmd,
                          kAddGalleryWatchRequestRuntimeError);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesGalleryWatchApiTest,
                       SetupGalleryChangedListenerWithoutWatchers) {
  // Add gallery watch listener.
  ExecuteCmdAndCheckReply(kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);

  // Modify gallery contents. Listener should not get called because add watch
  // request was not called.
  ExtensionTestMessageListener gallery_change_event_received(
      kGalleryChangedEventReceived, false /* no reply */);
  ASSERT_TRUE(AddNewFileInTestGallery());

  // Remove gallery watch listener.
  ExecuteCmdAndCheckReply(kRemoveGalleryChangedListenerCmd,
                          kRemoveGalleryChangedListenerOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesGalleryWatchApiTest,
                       SetupWatchOnInvalidGallery) {
  // Add gallery watch listener.
  ExecuteCmdAndCheckReply(kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);
  // Set up a invalid gallery watch.
  ExecuteCmdAndCheckReply(kSetupWatchOnInvalidGalleryCmd,
                          kAddGalleryWatchRequestFailed);
}
