// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/write_from_url_operation.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/url_loader_interceptor.h"

namespace extensions {
namespace image_writer {

namespace {

using content::BrowserThread;
using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Gt;
using testing::Lt;

const char kTestImageUrl[] = "http://localhost/test/image.zip";

}  // namespace

// This class gives us a generic Operation with the ability to set or inspect
// the current path to the image file.
class WriteFromUrlOperationForTest : public WriteFromUrlOperation {
 public:
  WriteFromUrlOperationForTest(
      base::WeakPtr<OperationManager> manager,
      const ExtensionId& extension_id,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote,
      GURL url,
      const std::string& hash,
      const std::string& storage_unit_id)
      : WriteFromUrlOperation(manager,
                              extension_id,
                              std::move(factory_remote),
                              url,
                              hash,
                              storage_unit_id,
                              base::FilePath(FILE_PATH_LITERAL("/var/tmp"))) {}

  void StartImpl() override {}

  // Following methods let us:
  // 1. Expose stages for testing.
  // 2. Make sure Operation methods are invoked on its task runner.
  void Start() {
    PostTask(base::BindOnce(&WriteFromUrlOperation::Start, this));
  }
  void GetDownloadTarget(const base::Closure& continuation) {
    PostTask(base::BindOnce(&WriteFromUrlOperation::GetDownloadTarget, this,
                            continuation));
  }

  void Download(const base::Closure& continuation) {
    PostTask(
        base::BindOnce(&WriteFromUrlOperation::Download, this, continuation));
  }

  void VerifyDownload(const base::Closure& continuation) {
    PostTask(base::BindOnce(&WriteFromUrlOperation::VerifyDownload, this,
                            continuation));
  }

  void Cancel() {
    PostTask(base::BindOnce(&WriteFromUrlOperation::Cancel, this));
  }

  // Helpers to set-up state for intermediate stages.
  void SetImagePath(const base::FilePath image_path) {
    image_path_ = image_path;
  }

  base::FilePath GetImagePath() { return image_path_; }

 private:
  ~WriteFromUrlOperationForTest() override {}
};

class ImageWriterWriteFromUrlOperationTest : public ImageWriterUnitTestBase {
 protected:
  ImageWriterWriteFromUrlOperationTest()
      : url_loader_interceptor_(base::BindRepeating(
            &ImageWriterWriteFromUrlOperationTest::HandleRequest,
            base::Unretained(this))),
        url_interception_count_(0),
        manager_(&test_profile_) {}

  void TearDown() override {
    ImageWriterUnitTestBase::TearDown();
  }

  // Intercepts network requests.
  bool HandleRequest(content::URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url == GURL(kTestImageUrl)) {
      url_interception_count_++;
      content::URLLoaderInterceptor::WriteResponse(test_utils_.GetImagePath(),
                                                   params->client.get());
      return true;
    }
    return false;
  }

  scoped_refptr<WriteFromUrlOperationForTest> CreateOperation(
      const GURL& url,
      const std::string& hash) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        url_loader_factory_remote;
    content::BrowserContext::GetDefaultStoragePartition(&test_profile_)
        ->GetURLLoaderFactoryForBrowserProcess()
        ->Clone(url_loader_factory_remote.InitWithNewPipeAndPassReceiver());

    scoped_refptr<WriteFromUrlOperationForTest> operation(
        new WriteFromUrlOperationForTest(
            manager_.AsWeakPtr(), kDummyExtensionId,
            std::move(url_loader_factory_remote), url, hash,
            test_utils_.GetDevicePath().AsUTF8Unsafe()));
    operation->Start();
    return operation;
  }

  TestingProfile test_profile_;
  content::URLLoaderInterceptor url_loader_interceptor_;
  int url_interception_count_;

  MockOperationManager manager_;
};

// Crashes on Tsan.  http://crbug.com/859317
#if defined(THREAD_SANITIZER)
#define MAYBE_SelectTargetWithoutExtension DISABLED_SelectTargetWithoutExtension
#define MAYBE_SelectTargetWithExtension DISABLED_SelectTargetWithExtension
#else
#define MAYBE_SelectTargetWithoutExtension SelectTargetWithoutExtension
#define MAYBE_SelectTargetWithExtension SelectTargetWithExtension
#endif
TEST_F(ImageWriterWriteFromUrlOperationTest, MAYBE_SelectTargetWithoutExtension) {
  scoped_refptr<WriteFromUrlOperationForTest> operation =
      CreateOperation(GURL("http://localhost/foo/bar"), "");

  base::RunLoop run_loop;
  operation->GetDownloadTarget(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(FILE_PATH_LITERAL("bar"),
            operation->GetImagePath().BaseName().value());

  operation->Cancel();
  content::RunAllTasksUntilIdle();
}

TEST_F(ImageWriterWriteFromUrlOperationTest, MAYBE_SelectTargetWithExtension) {
  scoped_refptr<WriteFromUrlOperationForTest> operation =
      CreateOperation(GURL("http://localhost/foo/bar.zip"), "");

  base::RunLoop run_loop;
  operation->GetDownloadTarget(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(FILE_PATH_LITERAL("bar.zip"),
            operation->GetImagePath().BaseName().value());

  operation->Cancel();
}
#undef MAYBE_SelectTargetWithoutExtension
#undef MAYBE_SelectTargetWithExtension


TEST_F(ImageWriterWriteFromUrlOperationTest, DownloadFile) {
  // This test actually triggers the URL fetch code, which will drain the
  // message queues while waiting for IO, thus we have to run until the
  // operation completes.
  base::RunLoop runloop;
  base::FilePath download_target_path;
  scoped_refptr<WriteFromUrlOperationForTest> operation =
      CreateOperation(GURL(kTestImageUrl), "");

  EXPECT_TRUE(base::CreateTemporaryFileInDir(test_utils_.GetTempDir(),
                                             &download_target_path));
  operation->SetImagePath(download_target_path);

  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_DOWNLOAD, 0))
      .Times(AnyNumber());
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_DOWNLOAD, 100))
      .Times(AnyNumber());

  operation->Download(runloop.QuitClosure());
  runloop.Run();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(base::ContentsEqual(test_utils_.GetImagePath(),
                                  operation->GetImagePath()));

  EXPECT_EQ(1, url_interception_count_);

  operation->Cancel();
}

TEST_F(ImageWriterWriteFromUrlOperationTest, VerifyFile) {
  std::unique_ptr<char[]> data_buffer(new char[kTestFileSize]);
  base::ReadFile(test_utils_.GetImagePath(), data_buffer.get(), kTestFileSize);
  base::MD5Digest expected_digest;
  base::MD5Sum(data_buffer.get(), kTestFileSize, &expected_digest);
  std::string expected_hash = base::MD5DigestToBase16(expected_digest);

  scoped_refptr<WriteFromUrlOperationForTest> operation =
      CreateOperation(GURL(""), expected_hash);

  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYDOWNLOAD, _))
      .Times(AtLeast(1));
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYDOWNLOAD, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId,
                         image_writer_api::STAGE_VERIFYDOWNLOAD,
                         100)).Times(AtLeast(1));

  operation->SetImagePath(test_utils_.GetImagePath());
  {
    base::RunLoop run_loop;
    // The OnProgress tasks are posted with priority USER_VISIBLE priority so
    // post the quit closure with the same priority to ensure it doesn't run too
    // soon.
    operation->VerifyDownload(base::Bind(
        [](base::OnceClosure quit_closure) {
          base::PostTask(
              FROM_HERE,
              {content::BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
              std::move(quit_closure));
        },
        run_loop.QuitClosure()));

    run_loop.Run();
  }

  operation->Cancel();

  // The OnProgress calls we're expecting are posted to the Operation's
  // SequencedTaskRunner. Flush it before the mock's expectations are checked.
  {
    base::RunLoop run_loop;
    operation->PostTask(run_loop.QuitClosure());
    run_loop.Run();
  }
}

}  // namespace image_writer
}  // namespace extensions
