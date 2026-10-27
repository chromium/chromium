// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"

namespace extensions::image_writer {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Lt;

namespace {

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
void SetUpImageWriteClientProgressSimulation(FakeImageWriterClient* client) {
  std::vector<int> progress_list{0, 50, 100};
  bool will_succeed = true;
  client->SimulateProgressOnWrite(progress_list, will_succeed);
  client->SimulateProgressOnVerifyWrite(progress_list, will_succeed);
}
#endif

}  // namespace

class ImageWriterFromFileTest : public ImageWriterUnitTestBase {
 protected:
  ImageWriterFromFileTest()
      : profile_(new TestingProfile), manager_(profile_.get()) {}
  std::unique_ptr<TestingProfile> profile_;
  MockOperationManager manager_;
};

TEST_F(ImageWriterFromFileTest, InvalidFile) {
  scoped_refptr<WriteFromFileOperation> op = new WriteFromFileOperation(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));

  base::DeleteFile(test_utils_.GetImagePath());

  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId, _, _)).Times(0);
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kUnknown, 0,
                      error::kImageInvalid))
      .Times(1);

  op->PostTask(base::BindOnce(&Operation::Start, op));
  content::RunAllTasksUntilIdle();
}

// Runs the entire WriteFromFile operation.
TEST_F(ImageWriterFromFileTest, WriteFromFileEndToEnd) {
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  // Sets up simulating Operation::Progress() and Operation::Success().
  test_utils_.RunOnUtilityClientCreation(
      base::BindOnce(&SetUpImageWriteClientProgressSimulation));
#endif

  scoped_refptr<WriteFromFileOperation> op = new WriteFromFileOperation(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kWrite, 100))
      .Times(AtLeast(1));

#if !BUILDFLAG(IS_CHROMEOS)
  // Chrome OS doesn't verify.
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, 100))
      .Times(AtLeast(1));
#endif

  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(1);
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);

  op->PostTask(base::BindOnce(&Operation::Start, op));
  content::RunAllTasksUntilIdle();
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ImageWriterFromFileTest, LocalFileEmptyImageReportsImageSizeError) {
  base::FilePath empty_image =
      test_utils_.GetTempDir().AppendASCII("empty_image.bin");
  ASSERT_TRUE(base::WriteFile(empty_image, base::span<const uint8_t>()));

  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, empty_image,
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kWrite, _,
                      error::kImageSizeError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();
}

TEST_F(ImageWriterFromFileTest, LocalFileMissingDeviceReportsDeviceOpenError) {
  base::FilePath missing_device =
      test_utils_.GetTempDir().AppendASCII("nonexistent_dir/device.bin");

  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      missing_device.AsUTF8Unsafe(),
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kWrite, _,
                      error::kDeviceOpenError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();
}

TEST_F(ImageWriterFromFileTest, LocalFileCancelMidTransferStopsFurtherWrites) {
  base::FilePath large_image_path =
      test_utils_.GetTempDir().AppendASCII("large_image.bin");
  std::vector<uint8_t> large_image(256 * 1024, 'A');
  ASSERT_TRUE(base::WriteFile(large_image_path, large_image));
  // Truncate the pre-populated 32 KiB test device file to 0 bytes so the final
  // file size reflects only the bytes written before cancellation.
  ASSERT_TRUE(base::WriteFile(test_utils_.GetDevicePath(),
                              base::span<const uint8_t>()));

  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, large_image_path,
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));

  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);

  base::RunLoop run_loop;
  std::optional<int64_t> size_at_cancel;
  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  // `Operation::Start` posts `Extract`, which posts `Write` (writing the first
  // 64 KiB chunk and posting chunk 2). Enqueue `Operation::Cancel` at the same
  // task depth on `Operation::task_runner_` so it executes deterministically
  // right after chunk 1 and before chunk 2, capture the file size immediately
  // after `Cancel`, and then quit `run_loop` after chunk 2 observes
  // cancellation and exits.
  operation->PostTask(base::BindOnce(
      &Operation::PostTask, operation,
      base::BindOnce(
          &Operation::PostTask, operation,
          base::BindOnce(&Operation::Cancel, operation)
              .Then(base::BindOnce(
                  [](const base::FilePath& device_path,
                     std::optional<int64_t>* out_size,
                     scoped_refptr<Operation> op,
                     base::OnceClosure quit_closure) {
                    *out_size = base::GetFileSize(device_path);
                    op->PostTask(std::move(quit_closure));
                  },
                  test_utils_.GetDevicePath(), &size_at_cancel, operation,
                  run_loop.QuitClosure())))));
  run_loop.Run();

  ASSERT_TRUE(size_at_cancel.has_value());
  EXPECT_GT(*size_at_cancel, 0);
  EXPECT_LT(*size_at_cancel, static_cast<int64_t>(large_image.size()));

  std::optional<int64_t> destination_size =
      base::GetFileSize(test_utils_.GetDevicePath());
  ASSERT_TRUE(destination_size.has_value());
  EXPECT_EQ(*destination_size, *size_at_cancel);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace extensions::image_writer
