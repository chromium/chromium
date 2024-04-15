// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/long_screenshots/long_screenshots_tab_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace long_screenshots {

using paint_preview::DirectoryKey;
using paint_preview::FileManager;

namespace {

constexpr char kFeatureName[] = "tab_service_test";

// Override PaintPreviewRecorder with a mock version where the status and
// response can be manipulated based on the expected response.
class MockPaintPreviewRecorder
    : public paint_preview::mojom::PaintPreviewRecorder {
 public:
  MockPaintPreviewRecorder() = default;
  ~MockPaintPreviewRecorder() override = default;

  MockPaintPreviewRecorder(const MockPaintPreviewRecorder&) = delete;
  MockPaintPreviewRecorder& operator=(const MockPaintPreviewRecorder&) = delete;

  void CapturePaintPreview(
      paint_preview::mojom::PaintPreviewCaptureParamsPtr params,
      paint_preview::mojom::PaintPreviewRecorder::CapturePaintPreviewCallback
          callback) override {
    std::move(callback).Run(status_, std::move(response_));
  }

  // Must be called with a new `response` before each capture.
  void SetResponse(
      paint_preview::mojom::PaintPreviewStatus status,
      paint_preview::mojom::PaintPreviewCaptureResponsePtr&& response) {
    status_ = status;
    response_ = std::move(response);
  }

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.reset();
    binding_.Bind(
        mojo::PendingAssociatedReceiver<
            paint_preview::mojom::PaintPreviewRecorder>(std::move(handle)));
  }

 private:
  paint_preview::mojom::PaintPreviewStatus status_;
  paint_preview::mojom::PaintPreviewCaptureResponsePtr response_;
  mojo::AssociatedReceiver<paint_preview::mojom::PaintPreviewRecorder> binding_{
      this};
};

std::vector<base::FilePath> ListDir(const base::FilePath& path) {
  base::FileEnumerator enumerator(
      path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      FILE_PATH_LITERAL("*.skp"));  // Ignore the proto.pb files.
  std::vector<base::FilePath> files;
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    files.push_back(name);
  }
  return files;
}

}  // namespace

class LongScreenshotsTabServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  LongScreenshotsTabServiceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}
  ~LongScreenshotsTabServiceTest() override = default;

  LongScreenshotsTabServiceTest(const LongScreenshotsTabServiceTest&) = delete;
  LongScreenshotsTabServiceTest& operator=(
      const LongScreenshotsTabServiceTest&) = delete;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://www.example.com/"),
                      ui::PageTransition::PAGE_TRANSITION_FIRST);
    task_environment()->RunUntilIdle();
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    service_ = std::make_unique<LongScreenshotsTabService>(
        std::make_unique<paint_preview::PaintPreviewFileMixin>(
            temp_dir_.GetPath(), kFeatureName),
        nullptr, false);
    task_environment()->RunUntilIdle();
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  LongScreenshotsTabService* GetService() { return service_.get(); }

  bool TabService_IsAmpUrl(const GURL& url) { return service_->IsAmpUrl(url); }

  content::RenderFrameHost* TabService_GetRootRenderFrameHost(
      content::RenderFrameHost* frame,
      const GURL& url) {
    return service_->GetRootRenderFrameHost(frame, url);
  }

  void OverrideInterface(MockPaintPreviewRecorder* recorder) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        paint_preview::mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&MockPaintPreviewRecorder::BindRequest,
                            base::Unretained(recorder)));
  }

  const base::FilePath& GetPath() const { return temp_dir_.GetPath(); }

 private:
  std::unique_ptr<LongScreenshotsTabService> service_;
  base::ScopedTempDir temp_dir_;
};

// Test a successful capturing of a tab.
TEST_F(LongScreenshotsTabServiceTest, CaptureTab) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(
      paint_preview::mojom::PaintPreviewStatus::kOk,
      paint_preview::mojom::PaintPreviewCaptureResponse::New());
  OverrideInterface(&recorder);

  auto* service = GetService();
  service->CaptureTab(kTabId, GURL::EmptyGURL(), web_contents(), 0, 0, 1000,
                      1000, false);
  task_environment()->RunUntilIdle();

  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&paint_preview::FileManager::DirectoryExists, file_manager,
                     key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  task_environment()->RunUntilIdle();

  service->DeleteAllLongScreenshotFiles();
  task_environment()->RunUntilIdle();
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
}

// Test a successful capturing of a tab in memory.
TEST_F(LongScreenshotsTabServiceTest, CaptureTabInMemory) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  paint_preview::mojom::PaintPreviewCaptureResponsePtr response =
      paint_preview::mojom::PaintPreviewCaptureResponse::New();
  response->skp.emplace(mojo_base::BigBuffer());
  recorder.SetResponse(paint_preview::mojom::PaintPreviewStatus::kOk,
                       std::move(response));
  OverrideInterface(&recorder);

  auto* service = GetService();
  service->CaptureTab(kTabId, GURL::EmptyGURL(), web_contents(), 0, 0, 1000,
                      1000, true);
  task_environment()->RunUntilIdle();

  // No file should have been created.
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&paint_preview::FileManager::DirectoryExists, file_manager,
                     key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
}

// Test a successful capturing a tab multiple times.
TEST_F(LongScreenshotsTabServiceTest, CaptureTabTwice) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(
      paint_preview::mojom::PaintPreviewStatus::kOk,
      paint_preview::mojom::PaintPreviewCaptureResponse::New());
  OverrideInterface(&recorder);

  auto* service = GetService();
  service->CaptureTab(kTabId, GURL::EmptyGURL(), web_contents(), 0, 0, 1000,
                      1000, false);

  task_environment()->RunUntilIdle();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  task_environment()->RunUntilIdle();
  base::FilePath path_1;
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, file_manager, key,
                     false),
      base::BindOnce(
          [](base::FilePath* out, const std::optional<base::FilePath>& path) {
            EXPECT_TRUE(path.has_value());
            *out = path.value();
          },
          &path_1));
  task_environment()->RunUntilIdle();
  auto files_1 = ListDir(path_1);
  ASSERT_EQ(1U, files_1.size());

  recorder.SetResponse(
      paint_preview::mojom::PaintPreviewStatus::kOk,
      paint_preview::mojom::PaintPreviewCaptureResponse::New());
  service->CaptureTab(kTabId, GURL::EmptyGURL(), web_contents(), 1000, 1000,
                      2000, 2000, false);
  task_environment()->RunUntilIdle();

  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  base::FilePath path_2;
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, file_manager, key,
                     false),
      base::BindOnce(
          [](base::FilePath* out, const std::optional<base::FilePath>& path) {
            EXPECT_TRUE(path.has_value());
            *out = path.value();
          },
          &path_2));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(path_2, path_1);
  auto files_2 = ListDir(path_2);
  ASSERT_EQ(1U, files_2.size());
  // The embedding token is used in the filename of the captured SkPicture.
  // Since the embedding token is the same the filenames should also be the
  // same.
  EXPECT_EQ(files_1, files_2);

  service->DeleteAllLongScreenshotFiles();
  task_environment()->RunUntilIdle();
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
}

// Test when PaintPreviewRecorder returns a failure status code.
TEST_F(LongScreenshotsTabServiceTest, CaptureTabFailed) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(
      paint_preview::mojom::PaintPreviewStatus::kFailed,
      paint_preview::mojom::PaintPreviewCaptureResponse::New());
  OverrideInterface(&recorder);

  auto* service = GetService();
  service->CaptureTab(kTabId, GURL::EmptyGURL(), web_contents(), 0, 0, 1000,
                      1000, false);
  task_environment()->RunUntilIdle();

  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  task_environment()->RunUntilIdle();

  service->DeleteAllLongScreenshotFiles();
  task_environment()->RunUntilIdle();
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
}

TEST_F(LongScreenshotsTabServiceTest, AmpPageDetection) {
  GURL amp_url("https://www.google.com/amp/s/example");
  GURL amp_cahe_url(
      "https://www-example-com.cdn.ampproject.org/c/www.example.com");
  GURL google_news_url("https://news.google.com/articles/foo");
  GURL non_amp_url("https://foo.com/amp/bar/");

  ASSERT_TRUE(TabService_IsAmpUrl(amp_url));
  ASSERT_TRUE(TabService_IsAmpUrl(amp_cahe_url));
  ASSERT_TRUE(TabService_IsAmpUrl(google_news_url));
  ASSERT_FALSE(TabService_IsAmpUrl(non_amp_url));

  auto* rfh = main_rfh();
  ASSERT_EQ(rfh, TabService_GetRootRenderFrameHost(rfh, non_amp_url));
  // The main frame has no child frame so main frame should be returned.
  ASSERT_EQ(rfh, TabService_GetRootRenderFrameHost(rfh, amp_url));
  auto* rfh_tester = content::RenderFrameHostTester::For(rfh);
  auto* subframe = rfh_tester->AppendChild("child 1");
  // Child frame should be returned.
  ASSERT_EQ(subframe, TabService_GetRootRenderFrameHost(rfh, amp_url));
  // main frame has more than one child frame. Main frame should be returned.
  rfh_tester->AppendChild("child 2");
  ASSERT_EQ(rfh, TabService_GetRootRenderFrameHost(rfh, amp_url));
}

}  // namespace long_screenshots
