// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/image_writer_utility_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"

namespace extensions {
namespace image_writer {

namespace {
constexpr int64_t kTestFileSize = 1 << 15;  // 32 kB
}  // namespace

class ImageWriterUtilityClientTest : public InProcessBrowserTest {
 public:
  ImageWriterUtilityClientTest() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    test_device_ = base::FilePath().AppendASCII(
        chrome::mojom::RemovableStorageWriter::kTestDevice);
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ImageWriterUtilityClientTest(const ImageWriterUtilityClientTest&) = delete;
  ImageWriterUtilityClientTest& operator=(const ImageWriterUtilityClientTest&) =
      delete;

  void FillImageFileWithPattern(char pattern) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &image_));

    base::RunLoop run_loop;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ImageWriterUtilityClientTest::FillFile, image_,
                       pattern),
        run_loop.QuitClosure());

    run_loop.Run();
  }

  void FillDeviceFileWithPattern(char pattern) {
    device_ = image_.ReplaceExtension(FILE_PATH_LITERAL("out"));

    base::RunLoop run_loop;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ImageWriterUtilityClientTest::FillFile, device_,
                       pattern),
        run_loop.QuitClosure());

    run_loop.Run();
  }

  enum RunOption { WRITE, VERIFY, CANCEL };

  void RunWriteTest(RunOption option = WRITE) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();

    verify_ = (option == VERIFY);
    cancel_ = (option == CANCEL);

    CreateTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ImageWriterUtilityClientTest::StartWriteTest,
                                  base::Unretained(this)));
    run_loop.Run();

    EXPECT_TRUE(quit_called_);
  }

  void RunVerifyTest(RunOption option = VERIFY) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();

    ASSERT_NE(option, WRITE);  // Verify tests do not WRITE.
    cancel_ = (option == CANCEL);
    CreateTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ImageWriterUtilityClientTest::StartVerifyTest,
                       base::Unretained(this)));
    run_loop.Run();

    EXPECT_TRUE(quit_called_);
  }

  bool success() const { return success_; }

  const std::string& error() const { return error_; }

 private:
  void StartWriteTest() {
    DCHECK(IsRunningInCorrectSequence());

    if (!image_writer_utility_client_) {
      image_writer_utility_client_ =
          new ImageWriterUtilityClient(GetTaskRunner());
    }
    success_ = false;
    progress_ = 0;

    image_writer_utility_client_->Write(
        base::BindRepeating(&ImageWriterUtilityClientTest::Progress,
                            base::Unretained(this)),
        base::BindOnce(&ImageWriterUtilityClientTest::Success,
                       base::Unretained(this)),
        base::BindOnce(&ImageWriterUtilityClientTest::Failure,
                       base::Unretained(this)),
        image_, test_device_);
  }

  void Progress(int64_t progress) {
    DCHECK(IsRunningInCorrectSequence());

    progress_ = progress;
    if (!cancel_) {
      return;
    }

    image_writer_utility_client_->Cancel(base::BindOnce(
        &ImageWriterUtilityClientTest::Cancelled, base::Unretained(this)));
  }

  void Success() {
    DCHECK(IsRunningInCorrectSequence());

    EXPECT_EQ(kTestFileSize, progress_);
    EXPECT_FALSE(cancel_);
    success_ = !cancel_;

    if (verify_) {
      StartVerifyTest();
      return;
    }

    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ImageWriterUtilityClientTest::Shutdown,
                                  base::Unretained(this)));
  }

  void StartVerifyTest() {
    DCHECK(IsRunningInCorrectSequence());

    if (!image_writer_utility_client_) {
      image_writer_utility_client_ =
          new ImageWriterUtilityClient(GetTaskRunner());
    }
    success_ = false;
    progress_ = 0;

    image_writer_utility_client_->Verify(
        base::BindRepeating(&ImageWriterUtilityClientTest::Progress,
                            base::Unretained(this)),
        base::BindOnce(&ImageWriterUtilityClientTest::Verified,
                       base::Unretained(this)),
        base::BindOnce(&ImageWriterUtilityClientTest::Failure,
                       base::Unretained(this)),
        image_, test_device_);
  }

  void Failure(const std::string& error) {
    DCHECK(IsRunningInCorrectSequence());

    EXPECT_FALSE(error.empty());
    success_ = false;
    error_ = error;

    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ImageWriterUtilityClientTest::Shutdown,
                                  base::Unretained(this)));
  }

  void Verified() {
    DCHECK(IsRunningInCorrectSequence());

    EXPECT_EQ(kTestFileSize, progress_);
    EXPECT_FALSE(cancel_);
    success_ = !cancel_;

    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ImageWriterUtilityClientTest::Shutdown,
                                  base::Unretained(this)));
  }

  void Cancelled() {
    DCHECK(IsRunningInCorrectSequence());

    EXPECT_TRUE(cancel_);
    success_ = cancel_;

    quit_called_ = true;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, quit_closure_);
  }

  void Shutdown() {
    DCHECK(IsRunningInCorrectSequence());

    image_writer_utility_client_->Shutdown();

    quit_called_ = true;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, quit_closure_);
  }

  static void FillFile(const base::FilePath& path, char pattern) {
    const std::string fill(kTestFileSize, pattern);
    EXPECT_TRUE(base::WriteFile(path, fill));
  }

  base::SequencedTaskRunner* CreateTaskRunner() {
    DCHECK(!task_runner_.get());
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        Operation::blocking_task_traits());
    return task_runner_.get();
  }

  base::SequencedTaskRunner* GetTaskRunner() {
    DCHECK(task_runner_.get())
        << "Called GetTaskRunner before creating TaskRunner.";
    return task_runner_.get();
  }

  bool IsRunningInCorrectSequence() const {
    return task_runner_->RunsTasksInCurrentSequence();
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath test_device_;
  base::FilePath device_;
  base::FilePath image_;

  base::RepeatingClosure quit_closure_;
  bool quit_called_ = false;

  // Lives on |task_runner_|.
  scoped_refptr<ImageWriterUtilityClient> image_writer_utility_client_;
  int64_t progress_ = 0;
  bool success_ = false;
  bool verify_ = false;
  bool cancel_ = false;
  std::string error_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, WriteNoImage) {
  RunWriteTest();

  EXPECT_FALSE(success());
  EXPECT_FALSE(error().empty());
}

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, WriteNoDevice) {
  FillImageFileWithPattern(0);

  RunWriteTest();

  EXPECT_FALSE(success());
  EXPECT_FALSE(error().empty());
}

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, Write) {
  FillImageFileWithPattern('i');
  FillDeviceFileWithPattern(0);

  RunWriteTest();

  EXPECT_TRUE(success());
  EXPECT_TRUE(error().empty());
}

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, WriteVerify) {
  FillImageFileWithPattern('m');
  FillDeviceFileWithPattern(0);

  RunWriteTest(VERIFY);

  EXPECT_TRUE(success());
  EXPECT_TRUE(error().empty());
}

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, WriteCancel) {
  FillImageFileWithPattern('a');
  FillDeviceFileWithPattern(0);

  RunWriteTest(CANCEL);

  EXPECT_TRUE(success());
  EXPECT_TRUE(error().empty());
}

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, VerifyNoImage) {
  RunVerifyTest();

  EXPECT_FALSE(success());
  EXPECT_FALSE(error().empty());
}

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, VerifyNoDevice) {
  FillImageFileWithPattern(0);

  RunVerifyTest();

  EXPECT_FALSE(success());
  EXPECT_FALSE(error().empty());
}

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, VerifyFailure) {
  FillImageFileWithPattern('g');
  FillDeviceFileWithPattern(0);

  RunVerifyTest();

  EXPECT_FALSE(success());
  EXPECT_FALSE(error().empty());
}

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, Verify) {
  FillImageFileWithPattern('e');
  FillDeviceFileWithPattern('e');

  RunVerifyTest();

  EXPECT_TRUE(success());
  EXPECT_TRUE(error().empty());
}

IN_PROC_BROWSER_TEST_F(ImageWriterUtilityClientTest, VerifyCancel) {
  FillImageFileWithPattern('s');
  FillDeviceFileWithPattern('s');

  RunVerifyTest(CANCEL);

  EXPECT_TRUE(success());
  EXPECT_TRUE(error().empty());
}

}  // namespace image_writer
}  // namespace extensions
