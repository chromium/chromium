// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/file_change_service_bridge_ash.h"

#include "base/scoped_observation.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/fileapi/file_change_service.h"
#include "chrome/browser/ash/fileapi/file_change_service_factory.h"
#include "chrome/browser/ash/fileapi/file_change_service_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/file_change_service_bridge.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace crosapi {
namespace {

// Aliases.
using ::ash::FileChangeService;
using ::ash::FileChangeServiceFactory;
using ::ash::FileChangeServiceObserver;
using ::crosapi::mojom::FileChangeServiceBridge;
using ::testing::Eq;
using ::testing::NiceMock;

// Helpers ---------------------------------------------------------------------

// Returns a `storage::FileSystemURL` for `profile` and the given `file_path`.
// Causes test failure if `file_path` cannot be converted to a file system URL.
storage::FileSystemURL CreateFileSystemURL(Profile* profile,
                                           const base::FilePath& file_path) {
  GURL url;
  EXPECT_TRUE(file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
      profile, file_path, file_manager::util::GetFileManagerURL(), &url));
  return file_manager::util::GetFileManagerFileSystemContext(profile)
      ->CrackURLInFirstPartyContext(url);
}

// MockFileChangeServiceObserver -----------------------------------------------

class MockFileChangeServiceObserver : public FileChangeServiceObserver {
 public:
  // FileChangeServiceObserver:
  MOCK_METHOD(void,
              OnFileCreatedFromShowSaveFilePicker,
              (const GURL& file_picker_binding_context,
               const storage::FileSystemURL& url),
              (override));
};

// FileChangeServiceBridgeAshBrowserTest ---------------------------------------

// Base class for tests of the `FileChangeServiceBridgeAsh`.
class FileChangeServiceBridgeAshBrowserTest : public InProcessBrowserTest {
 public:
  // Returns the remote `FileChangeServiceBridge` for use by crosapi.
  mojo::Remote<FileChangeServiceBridge>& file_change_service_bridge() {
    return file_change_service_bridge_;
  }

 private:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Bind the remote `FileChangeServiceBridge` for use by crosapi.
    CrosapiManager::Get()->crosapi_ash()->BindFileChangeServiceBridge(
        file_change_service_bridge_.BindNewPipeAndPassReceiver());
  }

  // The remote `FileChangeServiceBridge` bound for use by crosapi.
  mojo::Remote<FileChangeServiceBridge> file_change_service_bridge_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

// Verifies that `OnFileCreatedFromShowSaveFilePicker()` events are propagated.
IN_PROC_BROWSER_TEST_F(FileChangeServiceBridgeAshBrowserTest,
                       PropagatesOnFileCreatedFromShowSaveFilePickerEvents) {
  Profile* const profile = browser()->profile();

  // Observe the `FileChangeService` in Ash.
  NiceMock<MockFileChangeServiceObserver> observer;
  base::ScopedObservation<FileChangeService, FileChangeServiceObserver>
      observation(&observer);
  observation.Observe(
      FileChangeServiceFactory::GetInstance()->GetService(profile));

  // Create and cache metadata for a file creation event.
  const GURL file_picker_binding_context("http://example.com/foo");
  base::FilePath file_path = ash::test::CreateFile(profile);

  // Expect `observer` in Ash to be notified of file creation event from Lacros.
  EXPECT_CALL(observer, OnFileCreatedFromShowSaveFilePicker(
                            Eq(file_picker_binding_context),
                            Eq(CreateFileSystemURL(profile, file_path))));

  // Send a message over crosapi, as if from Lacros, to propagate the file
  // creation event to the `observer` in Ash.
  mojo::Remote<FileChangeServiceBridge>& file_change_service_bridge =
      this->file_change_service_bridge();
  file_change_service_bridge->OnFileCreatedFromShowSaveFilePicker(
      file_picker_binding_context, file_path);
  file_change_service_bridge.FlushForTesting();
}

}  // namespace crosapi
