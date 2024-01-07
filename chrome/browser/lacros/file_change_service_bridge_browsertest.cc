// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/file_change_service_bridge.h"

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/file_change_service_bridge.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Aliases.
using ::crosapi::mojom::FileChangeServiceBridge;
using ::testing::Eq;
using ::testing::NiceMock;

// MockFileChangeServiceBridge -------------------------------------------------

class MockFileChangeServiceBridge : public FileChangeServiceBridge {
 public:
  // FileChangeServiceBridge:
  MOCK_METHOD(void,
              OnFileCreatedFromShowSaveFilePicker,
              (const GURL& file_picker_binding_context,
               const base::FilePath& url),
              (override));
};

// FileChangeServiceBridgeBrowserTest ------------------------------------------

// Base class for tests of the `FileChangeServiceBridge`.
class FileChangeServiceBridgeBrowserTest : public InProcessBrowserTest {
 public:
  // Returns the mock `FileChangeServiceBridge` for use by crosapi.
  NiceMock<MockFileChangeServiceBridge>& file_change_service_bridge() {
    return file_change_service_bridge_;
  }

 private:
  // InProcessBrowserTest:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);

    // Inject the mock `FileChangeServiceBridge` for use by crosapi.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        file_change_service_bridge_receiver_
            .BindNewPipeAndPassRemoteWithVersion());
  }

  // The mock `FileChangeServiceBridge` injected for use by crosapi.
  NiceMock<MockFileChangeServiceBridge> file_change_service_bridge_;
  mojo::Receiver<FileChangeServiceBridge> file_change_service_bridge_receiver_{
      &file_change_service_bridge_};
};

}  // namespace

// Tests -----------------------------------------------------------------------

// Verifies that `OnFileCreatedFromShowSaveFilePicker()` events are propagated.
IN_PROC_BROWSER_TEST_F(FileChangeServiceBridgeBrowserTest,
                       PropagatesOnFileCreatedFromShowSaveFilePickerEvents) {
  // Create and cache metadata for a file creation event.
  const GURL file_picker_binding_context("http://example.com/foo");
  const base::FilePath file_path("bar");
  const storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      blink::StorageKey(), storage::FileSystemType::kFileSystemTypeTest,
      file_path);

  // Expect the `file_change_service_bridge()` in Ash to be notified of file
  // creation event from Lacros.
  EXPECT_CALL(file_change_service_bridge(),
              OnFileCreatedFromShowSaveFilePicker(
                  Eq(file_picker_binding_context), Eq(url.path())));

  // Notify the chrome file system access permission context in Lacros of the
  // file creation event to propagate the event to the
  // `file_change_service_bridge()` in Ash.
  FileSystemAccessPermissionContextFactory::GetForProfile(browser()->profile())
      ->OnFileCreatedFromShowSaveFilePicker(file_picker_binding_context, url);
}
