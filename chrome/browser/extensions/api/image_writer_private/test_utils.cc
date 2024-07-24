// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"

#include <string.h>

#include <utility>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/common/chrome_paths.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"  // nogncheck
#include "chromeos/ash/components/dbus/image_burner/fake_image_burner_client.h"
#include "chromeos/ash/components/dbus/image_burner/image_burner_client.h"
#include "chromeos/ash/components/disks/disk.h"
#endif

namespace extensions {
namespace image_writer {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ImageWriterFakeImageBurnerClient : public ash::FakeImageBurnerClient {
 public:
  ImageWriterFakeImageBurnerClient() = default;
  ~ImageWriterFakeImageBurnerClient() override = default;

  void SetEventHandlers(
      BurnFinishedHandler burn_finished_handler,
      const BurnProgressUpdateHandler& burn_progress_update_handler) override {
    burn_finished_handler_ = std::move(burn_finished_handler);
    burn_progress_update_handler_ = burn_progress_update_handler;
  }

  void BurnImage(const std::string& from_path,
                 const std::string& to_path,
                 ErrorCallback error_callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(burn_progress_update_handler_, to_path, 0, 100));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(burn_progress_update_handler_, to_path, 50, 100));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(burn_progress_update_handler_, to_path, 100, 100));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(burn_finished_handler_), to_path, true, ""));
  }

 private:
  BurnFinishedHandler burn_finished_handler_;
  BurnProgressUpdateHandler burn_progress_update_handler_;
};
#endif

MockOperationManager::MockOperationManager(content::BrowserContext* context)
    : OperationManager(context) {}
MockOperationManager::~MockOperationManager() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
FakeDiskMountManager::FakeDiskMountManager() {}
FakeDiskMountManager::~FakeDiskMountManager() = default;

void FakeDiskMountManager::UnmountDeviceRecursively(
    const std::string& device_path,
    UnmountDeviceRecursivelyCallbackType callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ash::MountError::kSuccess));
}
#endif

SimulateProgressInfo::SimulateProgressInfo(
    const std::vector<int>& progress_list,
    bool will_succeed)
    : progress_list(progress_list), will_succeed(will_succeed) {}

SimulateProgressInfo::~SimulateProgressInfo() = default;
SimulateProgressInfo::SimulateProgressInfo(const SimulateProgressInfo&) =
    default;

FakeImageWriterClient::FakeImageWriterClient()
    : ImageWriterUtilityClient(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}
FakeImageWriterClient::~FakeImageWriterClient() = default;

void FakeImageWriterClient::SimulateProgressAndCompletion(
    const SimulateProgressInfo& info) {
  for (int progress : info.progress_list)
    Progress(progress);
  if (info.will_succeed) {
    Success();
  } else {
    Error(error::kVerificationFailed);
  }
}

void FakeImageWriterClient::Write(ProgressCallback progress_callback,
                                  SuccessCallback success_callback,
                                  ErrorCallback error_callback,
                                  const base::FilePath& source,
                                  const base::FilePath& target) {
  progress_callback_ = std::move(progress_callback);
  success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  if (simulate_on_write_) {
    SimulateProgressAndCompletion(*simulate_on_write_);
    simulate_on_write_.reset();
  }
}

void FakeImageWriterClient::Verify(ProgressCallback progress_callback,
                                   SuccessCallback success_callback,
                                   ErrorCallback error_callback,
                                   const base::FilePath& source,
                                   const base::FilePath& target) {
  progress_callback_ = std::move(progress_callback);
  success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  if (simulate_on_verify_) {
    SimulateProgressAndCompletion(*simulate_on_verify_);
    simulate_on_verify_.reset();
  }
}

void FakeImageWriterClient::Cancel(CancelCallback cancel_callback) {
  cancel_callback_ = std::move(cancel_callback);
}

void FakeImageWriterClient::Shutdown() {
  // Clear handlers to not hold any reference to the caller.
  success_callback_.Reset();
  progress_callback_.Reset();
  error_callback_.Reset();
  cancel_callback_.Reset();

  simulate_on_write_.reset();
  simulate_on_verify_.reset();
}

void FakeImageWriterClient::SimulateProgressOnWrite(
    const std::vector<int>& progress_list,
    bool will_succeed) {
  simulate_on_write_ = SimulateProgressInfo(progress_list, will_succeed);
}

void FakeImageWriterClient::SimulateProgressOnVerifyWrite(
    const std::vector<int>& progress_list,
    bool will_succeed) {
  simulate_on_verify_ = SimulateProgressInfo(progress_list, will_succeed);
}

void FakeImageWriterClient::Progress(int64_t progress) {
  if (!progress_callback_.is_null())
    progress_callback_.Run(progress);
}

void FakeImageWriterClient::Success() {
  if (!success_callback_.is_null())
    std::move(success_callback_).Run();
}

void FakeImageWriterClient::Error(const std::string& message) {
  if (!error_callback_.is_null())
    std::move(error_callback_).Run(message);
}

void FakeImageWriterClient::Cancel() {
  if (!cancel_callback_.is_null())
    std::move(cancel_callback_).Run();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
scoped_refptr<ImageWriterUtilityClient> CreateFakeImageWriterUtilityClient(
    ImageWriterTestUtils* utils) {
  auto* client = new FakeImageWriterClient();
  utils->OnUtilityClientCreated(client);
  return base::WrapRefCounted(client);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

ImageWriterTestUtils::ImageWriterTestUtils()
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    : utility_client_factory_(
          base::BindRepeating(&CreateFakeImageWriterUtilityClient, this))
#endif
{
}

ImageWriterTestUtils::~ImageWriterTestUtils() = default;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void ImageWriterTestUtils::OnUtilityClientCreated(
    FakeImageWriterClient* client) {
  DCHECK(!client_.get())
      << "Single FakeImageWriterClient instance per test case expected.";
  client_ = client;
  if (!client_creation_callback_.is_null())
    std::move(client_creation_callback_).Run(client);
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void ImageWriterTestUtils::RunOnUtilityClientCreation(
    base::OnceCallback<void(FakeImageWriterClient*)> closure) {
  client_creation_callback_ = std::move(closure);
}
#endif

void ImageWriterTestUtils::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_image_path_));
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_device_path_));

  ASSERT_TRUE(FillFile(test_image_path_, kImagePattern, kTestFileSize));
  ASSERT_TRUE(FillFile(test_device_path_, kDevicePattern, kTestFileSize));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Browser tests might have already initialized ConciergeClient.
  if (!ash::ConciergeClient::Get()) {
    ash::ConciergeClient::InitializeFake(
        /*fake_cicerone_client=*/nullptr);
    concierge_client_initialized_ = true;
  }
  image_burner_client_ = std::make_unique<ImageWriterFakeImageBurnerClient>();
  ash::ImageBurnerClient::SetInstanceForTest(image_burner_client_.get());

  FakeDiskMountManager* disk_manager = new FakeDiskMountManager();
  ash::disks::DiskMountManager::InitializeForTesting(disk_manager);

  // Adds a disk entry for test_device_path_ with the same device and file path.
  disk_manager->CreateDiskEntryForMountDevice(
      {test_device_path_.value(), "/dummy/mount", ash::MountType::kDevice},
      "device_id", "device_label", "Vendor", "Product", ash::DeviceType::kUSB,
      kTestFileSize, true, true, true, false, kTestFileSystemType);
  disk_manager->SetupDefaultReplies();
#else
  ImageWriterUtilityClient::SetFactoryForTesting(&utility_client_factory_);
#endif
}

void ImageWriterTestUtils::TearDown() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ImageBurnerClient::SetInstanceForTest(nullptr);
  image_burner_client_.reset();

  if (concierge_client_initialized_) {
    ash::ConciergeClient::Shutdown();
    concierge_client_initialized_ = false;
  }
  ash::disks::DiskMountManager::Shutdown();
#else
  ImageWriterUtilityClient::SetFactoryForTesting(nullptr);
#endif
}

const base::FilePath& ImageWriterTestUtils::GetTempDir() {
  return temp_dir_.GetPath();
}

const base::FilePath& ImageWriterTestUtils::GetImagePath() {
  return test_image_path_;
}

const base::FilePath& ImageWriterTestUtils::GetDevicePath() {
  return test_device_path_;
}

bool ImageWriterTestUtils::ImageWrittenToDevice() {
  auto image_buffer = base::HeapArray<char>::Uninit(kTestFileSize);
  auto device_buffer = base::HeapArray<char>::Uninit(kTestFileSize);

  std::optional<uint64_t> image_bytes_read =
      ReadFile(test_image_path_, image_buffer);

  if (!image_bytes_read) {
    return false;
  }

  std::optional<uint64_t> device_bytes_read =
      ReadFile(test_device_path_, device_buffer);

  if (image_bytes_read != device_bytes_read) {
    return false;
  }

  return memcmp(image_buffer.data(), device_buffer.data(), *image_bytes_read) ==
         0;
}

bool ImageWriterTestUtils::FillFile(const base::FilePath& file,
                                    uint8_t pattern,
                                    size_t length) {
  std::vector<uint8_t> buffer(length, pattern);
  return base::WriteFile(file, buffer);
}

ImageWriterUnitTestBase::ImageWriterUnitTestBase()
    : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
ImageWriterUnitTestBase::~ImageWriterUnitTestBase() = default;

void ImageWriterUnitTestBase::SetUp() {
  testing::Test::SetUp();
  test_utils_.SetUp();
}

void ImageWriterUnitTestBase::TearDown() {
  testing::Test::TearDown();
  test_utils_.TearDown();
}

bool GetTestDataDirectory(base::FilePath* path) {
  bool success = base::PathService::Get(chrome::DIR_TEST_DATA, path);
  if (!success)
    return false;
  *path = path->AppendASCII("image_writer_private");
  return true;
}

}  // namespace image_writer
}  // namespace extensions
