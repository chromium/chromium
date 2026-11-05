// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/write_from_url_operation.h"

#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/url_loader_interceptor.h"
#include "crypto/obsolete/md5.h"

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
  void GetDownloadTarget(base::OnceClosure continuation) {
    PostTask(base::BindOnce(&WriteFromUrlOperation::GetDownloadTarget, this,
                            std::move(continuation)));
  }

  void Download(base::OnceClosure continuation) {
    PostTask(base::BindOnce(&WriteFromUrlOperation::Download, this,
                            std::move(continuation)));
  }

  void VerifyDownload(base::OnceClosure continuation) {
    PostTask(base::BindOnce(&WriteFromUrlOperation::VerifyDownload, this,
                            std::move(continuation)));
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
  ~WriteFromUrlOperationForTest() override = default;
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
    test_profile_.GetDefaultStoragePartition()
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

  void RunVerifyDownloadAndFlush(
      scoped_refptr<WriteFromUrlOperationForTest> operation) {
    {
      base::RunLoop run_loop;
      // The OnProgress tasks are posted with USER_VISIBLE priority so post the
      // quit closure with the same priority to ensure it doesn't run too soon.
      operation->VerifyDownload(base::BindOnce(
          [](base::OnceClosure quit_closure) {
            content::GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
                ->PostTask(FROM_HERE, std::move(quit_closure));
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

  TestingProfile test_profile_;
  content::URLLoaderInterceptor url_loader_interceptor_;
  int url_interception_count_;

  MockOperationManager manager_;
};

TEST_F(ImageWriterWriteFromUrlOperationTest, SelectTargetWithoutExtension) {
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

TEST_F(ImageWriterWriteFromUrlOperationTest, SelectTargetWithExtension) {
  scoped_refptr<WriteFromUrlOperationForTest> operation =
      CreateOperation(GURL("http://localhost/foo/bar.zip"), "");

  base::RunLoop run_loop;
  operation->GetDownloadTarget(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(FILE_PATH_LITERAL("bar.zip"),
            operation->GetImagePath().BaseName().value());

  operation->Cancel();
}

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

  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kDownload, 0))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kDownload, 100))
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
  auto buffer = base::HeapArray<uint8_t>::Uninit(kTestFileSize);
  base::ReadFile(test_utils_.GetImagePath(), buffer);
  std::string expected_hash = base::HexEncodeLower(
      crypto::obsolete::Md5::HashForTesting(buffer.as_span()));

  scoped_refptr<WriteFromUrlOperationForTest> operation =
      CreateOperation(GURL(""), expected_hash);

  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyDownload, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId,
                         image_writer_api::Stage::kVerifyDownload, 100))
      .Times(AtLeast(1));

  operation->SetImagePath(test_utils_.GetImagePath());
  RunVerifyDownloadAndFlush(operation);
}

TEST_F(ImageWriterWriteFromUrlOperationTest, VerifyMultiChunkLargeFile) {
  // Exceeds the 256 KiB MD5 chunk buffer across >2 chunks with an unaligned
  // trailing chunk.
  constexpr size_t kLargeImageSize = 600 * 1024 + 137;
  auto large_buffer = base::HeapArray<uint8_t>::Uninit(kLargeImageSize);
  for (size_t i = 0; i < kLargeImageSize; ++i) {
    large_buffer[i] = static_cast<uint8_t>((i * 31 + 17) & 0xFF);
  }
  base::FilePath large_image_path =
      test_utils_.GetTempDir().AppendASCII("large_verify.bin");
  ASSERT_TRUE(base::WriteFile(large_image_path, large_buffer.as_span()));

  const std::string expected_hash = base::HexEncodeLower(
      crypto::obsolete::Md5::HashForTesting(large_buffer.as_span()));

  scoped_refptr<WriteFromUrlOperationForTest> operation =
      CreateOperation(GURL(""), expected_hash);

  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyDownload, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyDownload,
                                   testing::AllOf(Gt(0), Lt(100))))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId,
                         image_writer_api::Stage::kVerifyDownload, 100))
      .Times(AtLeast(1));

  operation->SetImagePath(large_image_path);
  RunVerifyDownloadAndFlush(operation);
}

TEST_F(ImageWriterWriteFromUrlOperationTest, VerifyEmptyFile) {
  base::FilePath empty_image_path =
      test_utils_.GetTempDir().AppendASCII("empty_verify.bin");
  ASSERT_TRUE(base::WriteFile(empty_image_path, base::span<const uint8_t>()));

  const std::string expected_hash = base::HexEncodeLower(
      crypto::obsolete::Md5::HashForTesting(base::span<const uint8_t>()));

  scoped_refptr<WriteFromUrlOperationForTest> operation =
      CreateOperation(GURL(""), expected_hash);

  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyDownload, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId,
                         image_writer_api::Stage::kVerifyDownload, 100))
      .Times(AtLeast(1));

  operation->SetImagePath(empty_image_path);
  RunVerifyDownloadAndFlush(operation);
}

}  // namespace image_writer
}  // namespace extensions
