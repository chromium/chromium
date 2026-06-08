// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_print_manager.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/web_contents_tester.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class PrintManagerPeer : public printing::PrintManager {
 public:
  static int GetCookie(printing::PrintManager* manager) {
    return static_cast<PrintManagerPeer*>(manager)->cookie();
  }
};

class AwPrintManagerTest : public testing::Test {
 public:
  AwPrintManagerTest() = default;
  ~AwPrintManagerTest() override = default;

  void SetUp() override {
    test_content_client_initializer_ =
        std::make_unique<content::TestContentClientInitializer>();
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        browser_context_.get(), nullptr);
    ASSERT_TRUE(web_contents_)
        << "WebContentsTester::CreateTestWebContents returned null!";
    AwPrintManager::CreateForWebContents(web_contents_.get());
  }

  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    test_content_client_initializer_.reset();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(AwPrintManagerTest, FdIsResetOncePrintingStarts) {
  auto* print_manager = AwPrintManager::FromWebContents(web_contents());
  ASSERT_TRUE(print_manager);

  int test_fd = 123;
  auto settings = std::make_unique<printing::PrintSettings>();

  bool callback_called = false;
  print_manager->UpdateParam(
      std::move(settings), test_fd,
      base::BindLambdaForTesting(
          [&callback_called](int page_count) { callback_called = true; }));

  auto params = printing::mojom::DidPrintDocumentParams::New();
  params->document_cookie = 0;  // Wrong cookie to fail early
  params->content = printing::mojom::DidPrintContentParams::New();

  auto* host = static_cast<printing::mojom::PrintManagerHost*>(print_manager);

  bool did_print_callback_called = false;

  // We pass a wrong cookie, which causes DidPrintDocument to fail early.
  // DidPrintDocument will call PdfWritingDone(0).
  // Inside PdfWritingDone(), it CHECKs that fd_ is set to base::kInvalidFd.
  // If fd_ was not reset when printing started (i.e. at the beginning of
  // DidPrintDocument), the test will crash.
  host->DidPrintDocument(
      std::move(params),
      base::BindLambdaForTesting([&did_print_callback_called](bool success) {
        did_print_callback_called = true;
        EXPECT_FALSE(success);
      }));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(did_print_callback_called);
}

TEST_F(AwPrintManagerTest, FdIsResetAfterSuccessfulPrint) {
  auto* print_manager = AwPrintManager::FromWebContents(web_contents());
  ASSERT_TRUE(print_manager);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &temp_file));
  base::File file(temp_file, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  int valid_fd = file.GetPlatformFile();

  auto settings = std::make_unique<printing::PrintSettings>();

  bool callback_called = false;
  print_manager->UpdateParam(
      std::move(settings), valid_fd,
      base::BindLambdaForTesting([&callback_called](int page_count) {
        callback_called = true;
        EXPECT_EQ(1, page_count);
      }));

  auto params = printing::mojom::DidPrintDocumentParams::New();
  params->document_cookie = PrintManagerPeer::GetCookie(print_manager);

  auto content = printing::mojom::DidPrintContentParams::New();
  const uint8_t test_data[] = "test print data";
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(sizeof(test_data));
  ASSERT_TRUE(mapped_region.IsValid());
  mapped_region.mapping.GetMemoryAsSpan<uint8_t>().copy_from(test_data);

  content->metafile_data_region = std::move(mapped_region.region);
  params->content = std::move(content);

  auto* host = static_cast<printing::mojom::PrintManagerHost*>(print_manager);

  host->DidGetPrintedPagesCount(PrintManagerPeer::GetCookie(print_manager), 1);

  base::RunLoop run_loop;
  bool did_print_callback_called = false;

  host->DidPrintDocument(
      std::move(params),
      base::BindLambdaForTesting(
          [&did_print_callback_called,
           quit_closure = run_loop.QuitClosure()](bool success) mutable {
            did_print_callback_called = true;
            EXPECT_TRUE(success);
            std::move(quit_closure).Run();
          }));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(did_print_callback_called);
}

}  // namespace android_webview
