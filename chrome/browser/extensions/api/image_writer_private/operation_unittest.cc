// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {
namespace image_writer {

namespace {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Gt;
using testing::Lt;

#if !BUILDFLAG(IS_CHROMEOS_ASH)

void SetUpUtilityClientProgressOnVerifyWrite(
    const std::vector<int>& progress_list,
    bool will_succeed,
    FakeImageWriterClient* client) {
  client->SimulateProgressOnVerifyWrite(progress_list, will_succeed);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

// This class gives us a generic Operation with the ability to set or inspect
// the current path to the image file.
class OperationForTest : public Operation {
 public:
  OperationForTest(base::WeakPtr<OperationManager> manager_,
                   const ExtensionId& extension_id,
                   const std::string& device_path,
                   const base::FilePath& download_path)
      : Operation(manager_, extension_id, device_path, download_path) {}

  void StartImpl() override {}

  // Expose internal stages for testing.
  // Also wraps Operation's methods to run on correct sequence.
  void Extract(base::OnceClosure continuation) {
    PostTask(
        base::BindOnce(&Operation::Extract, this, std::move(continuation)));
  }

  void Write(base::OnceClosure continuation) {
    PostTask(base::BindOnce(&Operation::Write, this, std::move(continuation)));
  }

  void VerifyWrite(base::OnceClosure continuation) {
    PostTask(
        base::BindOnce(&Operation::VerifyWrite, this, std::move(continuation)));
  }

  void Start() { PostTask(base::BindOnce(&Operation::Start, this)); }

  void Cancel() { PostTask(base::BindOnce(&Operation::Cancel, this)); }

  // Helpers to set-up state for intermediate stages.
  void SetImagePath(const base::FilePath image_path) {
    image_path_ = image_path;
  }

  base::FilePath GetImagePath() { return image_path_; }

 private:
  ~OperationForTest() override {}
};

class ImageWriterOperationTest : public ImageWriterUnitTestBase {
 protected:
  ImageWriterOperationTest()
      : profile_(new TestingProfile), manager_(profile_.get()) {}
  void SetUp() override {
    ImageWriterUnitTestBase::SetUp();

    // Create the zip file.
    base::FilePath image_dir = test_utils_.GetTempDir().AppendASCII("zip");
    ASSERT_TRUE(base::CreateDirectory(image_dir));
    ASSERT_TRUE(base::CreateTemporaryFileInDir(image_dir, &image_path_));

    test_utils_.FillFile(image_path_, kImagePattern, kTestFileSize);

    zip_file_ = test_utils_.GetTempDir().AppendASCII("test_image.zip");
    ASSERT_TRUE(zip::Zip(image_dir, zip_file_, true));

    // Operation setup.
    operation_ =
        new OperationForTest(manager_.AsWeakPtr(),
                             kDummyExtensionId,
                             test_utils_.GetDevicePath().AsUTF8Unsafe(),
                             base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
    operation_->SetImagePath(test_utils_.GetImagePath());
  }

  void TearDown() override {
    // Ensure all callbacks have been destroyed and cleanup occurs.

    // Cancel() will ensure we Shutdown() FakeImageWriterClient.
    operation_->Cancel();
    task_environment_.RunUntilIdle();

    ImageWriterUnitTestBase::TearDown();
  }

  base::FilePath image_path_;
  base::FilePath zip_file_;

  std::unique_ptr<TestingProfile> profile_;

  MockOperationManager manager_;
  scoped_refptr<OperationForTest> operation_;
};

// Unizpping a non-zip should do nothing.
TEST_F(ImageWriterOperationTest, ExtractNonZipFile) {
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId, _, _)).Times(0);

  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId, _, _)).Times(0);
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);

  operation_->Start();
  base::RunLoop run_loop;
  operation_->Extract(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ImageWriterOperationTest, ExtractZipFile) {
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kUnzip, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kUnzip, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kUnzip, 100))
      .Times(AtLeast(1));

  operation_->SetImagePath(zip_file_);

  operation_->Start();
  base::RunLoop run_loop;
  operation_->Extract(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(base::ContentsEqual(image_path_, operation_->GetImagePath()));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(ImageWriterOperationTest, WriteImageToDevice) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  auto set_up_utility_client_progress =
      [](const std::vector<int>& progress_list, bool will_succeed,
         FakeImageWriterClient* client) {
        client->SimulateProgressOnWrite(progress_list, will_succeed);
      };
  // Sets up client for simulating Operation::Progress() on Operation::Write.
  std::vector<int> progress_list{0, kTestFileSize / 2, kTestFileSize};
  test_utils_.RunOnUtilityClientCreation(base::BindOnce(
      set_up_utility_client_progress, progress_list, true /* will_succeed */));
#endif
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kWrite, 100))
      .Times(AtLeast(1));

  operation_->Start();
  base::RunLoop run_loop;
  operation_->Write(run_loop.QuitClosure());
  run_loop.Run();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Chrome OS doesn't support verification in the ImageBurner, so these two tests
// are skipped.

TEST_F(ImageWriterOperationTest, VerifyFileSuccess) {
  // Sets up client for simulating Operation::Progress() on
  // Operation::VerifyWrite.
  std::vector<int> progress_list{0, kTestFileSize / 2, kTestFileSize};
  test_utils_.RunOnUtilityClientCreation(
      base::BindOnce(&SetUpUtilityClientProgressOnVerifyWrite, progress_list,
                     true /* will_succeed */));
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, 100))
      .Times(AtLeast(1));

  test_utils_.FillFile(
      test_utils_.GetDevicePath(), kImagePattern, kTestFileSize);

  operation_->Start();
  base::RunLoop run_loop;
  operation_->VerifyWrite(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ImageWriterOperationTest, VerifyFileFailure) {
  // Sets up client for simulating Operation::Progress() on
  // Operation::VerifyWrite. Also simulates failure.
  std::vector<int> progress_list{0, kTestFileSize / 2};
  test_utils_.RunOnUtilityClientCreation(
      base::BindOnce(&SetUpUtilityClientProgressOnVerifyWrite, progress_list,
                     false /* will_succeed */));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, 100))
      .Times(0);
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_, OnError(kDummyExtensionId,
                                image_writer_api::Stage::kVerifyWrite, _, _))
      .Times(1);

  test_utils_.FillFile(
      test_utils_.GetDevicePath(), kDevicePattern, kTestFileSize);

  operation_->Start();
  operation_->VerifyWrite(base::DoNothing());
  content::RunAllTasksUntilIdle();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that on creation the operation_ has the expected state.
TEST_F(ImageWriterOperationTest, Creation) {
  EXPECT_EQ(0, operation_->GetProgress());
  EXPECT_EQ(image_writer_api::Stage::kUnknown, operation_->GetStage());
}

}  // namespace image_writer
}  // namespace extensions
