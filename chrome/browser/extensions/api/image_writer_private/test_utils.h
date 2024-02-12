// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TEST_UTILS_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/image_writer_private/image_writer_utility_client.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#endif

namespace extensions {
namespace image_writer {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ImageWriterFakeImageBurnerClient;
#endif

const char kDummyExtensionId[] = "DummyExtension";

// Default file size to use in tests.  Currently 32kB.
const size_t kTestFileSize = 32 * 1024;
// Pattern to use in the image file.
const uint8_t kImagePattern = 0x55;  // 01010101
// Pattern to use in the device file.
const uint8_t kDevicePattern = 0xAA;  // 10101010
// Disk file system type
const char kTestFileSystemType[] = "vfat";

// A mock around the operation manager for tracking callbacks.  Note that there
// are non-virtual methods on this class that should not be called in tests.
class MockOperationManager : public OperationManager {
 public:
  explicit MockOperationManager(content::BrowserContext* context);
  ~MockOperationManager() override;

  MOCK_METHOD3(OnProgress, void(const ExtensionId& extension_id,
                                image_writer_api::Stage stage,
                                int progress));
  // Callback for completion events.
  MOCK_METHOD1(OnComplete, void(const ExtensionId& extension_id));

  // Callback for error events.
  MOCK_METHOD4(OnError, void(const ExtensionId& extension_id,
                             image_writer_api::Stage stage,
                             int progress,
                             const std::string& error_message));
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A fake for the DiskMountManager that will successfully call the unmount
// callback.
class FakeDiskMountManager : public ash::disks::MockDiskMountManager {
 public:
  FakeDiskMountManager();
  ~FakeDiskMountManager() override;

  void UnmountDeviceRecursively(
      const std::string& device_path,
      UnmountDeviceRecursivelyCallbackType callback) override;

 private:
  Disks disks_;
};
#endif

struct SimulateProgressInfo {
  SimulateProgressInfo(const std::vector<int>& progress_list,
                       bool will_succeed);
  ~SimulateProgressInfo();
  SimulateProgressInfo(const SimulateProgressInfo&);
  std::vector<int> progress_list;
  bool will_succeed;
};

class FakeImageWriterClient : public ImageWriterUtilityClient {
 public:
  FakeImageWriterClient();

  void Write(ProgressCallback progress_callback,
             SuccessCallback success_callback,
             ErrorCallback error_callback,
             const base::FilePath& source,
             const base::FilePath& target) override;

  void Verify(ProgressCallback progress_callback,
              SuccessCallback success_callback,
              ErrorCallback error_callback,
              const base::FilePath& source,
              const base::FilePath& target) override;

  void Cancel(CancelCallback cancel_callback) override;

  void Shutdown() override;

  // Issues Operation::Progress() calls with items in |progress_list| on
  // Operation Write(). Sends Operation::Success() iff |will_succeed| is true,
  // otherwise issues an error.
  void SimulateProgressOnWrite(const std::vector<int>& progress_list,
                               bool will_succeed);
  // Same as SimulateProgressOnWrite, but applies to Operation::VerifyWrite().
  void SimulateProgressOnVerifyWrite(const std::vector<int>& progress_list,
                                     bool will_succeed);

  // Triggers the progress callback.
  void Progress(int64_t progress);
  // Triggers the success callback.
  void Success();
  // Triggers the error callback.
  void Error(const std::string& message);
  // Triggers the cancel callback.
  void Cancel();

 protected:
  ~FakeImageWriterClient() override;

 private:
  void SimulateProgressAndCompletion(const SimulateProgressInfo& info);

  ProgressCallback progress_callback_;
  SuccessCallback success_callback_;
  ErrorCallback error_callback_;
  CancelCallback cancel_callback_;

  std::optional<SimulateProgressInfo> simulate_on_write_;
  std::optional<SimulateProgressInfo> simulate_on_verify_;
};

class ImageWriterTestUtils {
 public:
  ImageWriterTestUtils();
  virtual ~ImageWriterTestUtils();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  using UtilityClientCreationCallback =
      base::OnceCallback<void(FakeImageWriterClient*)>;
  void RunOnUtilityClientCreation(UtilityClientCreationCallback callback);

  // Called when an instance of utility client is created.
  void OnUtilityClientCreated(FakeImageWriterClient* client);
#endif

  // Verifies that the data in image_path was written to the file at
  // device_path.  This is different from base::ContentsEqual because the device
  // may be larger than the image.
  bool ImageWrittenToDevice();

  // Fills |file| with |length| bytes of |pattern|, overwriting any existing
  // data.
  bool FillFile(const base::FilePath& file, uint8_t pattern, size_t length);

  // Set up the test utils, creating temporary folders and such.
  virtual void SetUp();

  virtual void TearDown();

  const base::FilePath& GetTempDir();
  const base::FilePath& GetImagePath();
  const base::FilePath& GetDevicePath();

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath test_image_path_;
  base::FilePath test_device_path_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ImageWriterFakeImageBurnerClient> image_burner_client_;
  bool concierge_client_initialized_ = false;
#else
  scoped_refptr<FakeImageWriterClient> client_;
  ImageWriterUtilityClient::ImageWriterUtilityClientFactory
      utility_client_factory_;
  base::OnceCallback<void(FakeImageWriterClient*)> client_creation_callback_;
#endif
};

// Base class for unit tests that manages creating image and device files.
class ImageWriterUnitTestBase : public testing::Test {
 protected:
  ImageWriterUnitTestBase();
  ~ImageWriterUnitTestBase() override;

  void SetUp() override;
  void TearDown() override;

  ImageWriterTestUtils test_utils_;

  content::BrowserTaskEnvironment task_environment_;
};

bool GetTestDataDirectory(base::FilePath* path);

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TEST_UTILS_H_
