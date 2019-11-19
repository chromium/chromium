// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/pattern.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/api_unittest.h"
namespace extensions {

using api::image_writer_private::RemovableStorageDevice;
using extension_function_test_utils::RunFunctionAndReturnError;
using extensions::image_writer::FakeImageWriterClient;

class ImageWriterPrivateApiTest : public ExtensionApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    test_utils_.SetUp(true);

    ASSERT_TRUE(test_utils_.FillFile(test_utils_.GetImagePath(),
                                     image_writer::kImagePattern,
                                     image_writer::kTestFileSize));
    ASSERT_TRUE(test_utils_.FillFile(test_utils_.GetDevicePath(),
                                     image_writer::kDevicePattern,
                                     image_writer::kTestFileSize));

    scoped_refptr<StorageDeviceList> device_list(new StorageDeviceList);

    RemovableStorageDevice expected1;
    expected1.vendor = "Vendor 1";
    expected1.model = "Model 1";
    expected1.capacity = image_writer::kTestFileSize;
    expected1.removable = true;
#if defined(OS_WIN)
    expected1.storage_unit_id = test_utils_.GetDevicePath().AsUTF8Unsafe();
#else
    expected1.storage_unit_id = test_utils_.GetDevicePath().value();
#endif

    RemovableStorageDevice expected2;
    expected2.vendor = "Vendor 2";
    expected2.model = "Model 2";
    expected2.capacity = image_writer::kTestFileSize << 2;
    expected2.removable = false;
#if defined(OS_WIN)
    expected2.storage_unit_id = test_utils_.GetDevicePath().AsUTF8Unsafe();
#else
    expected2.storage_unit_id = test_utils_.GetDevicePath().value();
#endif

    device_list->data.push_back(std::move(expected1));
    device_list->data.push_back(std::move(expected2));

    RemovableStorageProvider::SetDeviceListForTesting(device_list);
  }

  void TearDownInProcessBrowserTestFixture() override {
    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
    test_utils_.TearDown();
    RemovableStorageProvider::ClearDeviceListForTesting();
    FileSystemChooseEntryFunction::StopSkippingPickerForTest();
  }


 protected:
  image_writer::ImageWriterTestUtils test_utils_;
};

IN_PROC_BROWSER_TEST_F(ImageWriterPrivateApiTest, TestListDevices) {
  ASSERT_TRUE(RunExtensionTest("image_writer_private/list_devices"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ImageWriterPrivateApiTest, TestWriteFromFile) {
  FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
      "test_temp", test_utils_.GetTempDir());

  base::FilePath selected_image(test_utils_.GetImagePath());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &selected_image);

#if !defined(OS_CHROMEOS)
  auto set_up_utility_client_callbacks = [](FakeImageWriterClient* client) {
    std::vector<int> progress_list{0, 50, 100};
    client->SimulateProgressOnWrite(progress_list, true);
    client->SimulateProgressOnVerifyWrite(progress_list, true);
  };

  // Sets up client for simulating Operation::Progress() on Operation::Write and
  // Operation::VerifyWrite.
  test_utils_.RunOnUtilityClientCreation(
      base::BindOnce(set_up_utility_client_callbacks));
#endif

  ASSERT_TRUE(RunPlatformAppTest("image_writer_private/write_from_file"))
      << message_;
}
}  // namespace extensions
