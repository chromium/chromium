// Copyright 2020 The Chromium Authors. All rights reserved.
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
    std::move(callback).Run(
        status_, paint_preview::mojom::PaintPreviewCaptureResponse::New());
  }

  void SetResponse(paint_preview::mojom::PaintPreviewStatus status) {
    status_ = status;
  }

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(
        mojo::PendingAssociatedReceiver<
            paint_preview::mojom::PaintPreviewRecorder>(std::move(handle)));
  }

 private:
  paint_preview::mojom::PaintPreviewStatus status_;
  mojo::AssociatedReceiver<paint_preview::mojom::PaintPreviewRecorder> binding_{
      this};
};

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

  void OverrideInterface(MockPaintPreviewRecorder* recorder) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces();
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
  recorder.SetResponse(paint_preview::mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&recorder);

  auto* service = GetService();
  service->CaptureTab(kTabId, web_contents());
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

// Test when PaintPreviewRecorder returns a failure status code.
TEST_F(LongScreenshotsTabServiceTest, CaptureTabFailed) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(paint_preview::mojom::PaintPreviewStatus::kFailed);
  OverrideInterface(&recorder);

  auto* service = GetService();
  service->CaptureTab(kTabId, web_contents());
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
}  // namespace long_screenshots
