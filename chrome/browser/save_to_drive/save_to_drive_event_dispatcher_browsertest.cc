// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_event_dispatcher.h"

#include "base/memory/raw_ptr.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/save_to_drive/save_to_drive_recorder.h"
#include "chrome/browser/save_to_drive/time_remaining_calculator.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/constants.h"
#include "pdf/pdf_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace save_to_drive {

namespace {
namespace pdf_api = extensions::api::pdf_viewer_private;
using ::testing::StrictMock;
}  // namespace

class MockSaveToDriveRecorder : public SaveToDriveRecorder {
 public:
  MockSaveToDriveRecorder() : SaveToDriveRecorder(nullptr) {}
  ~MockSaveToDriveRecorder() override = default;

  MOCK_METHOD(void,
              Record,
              (const pdf_api::SaveToDriveProgress& progress),
              (override));
};

class MockTimeRemainingCalculator : public TimeRemainingCalculator {
 public:
  MOCK_METHOD(std::optional<std::u16string>,
              CalculateTimeRemainingText,
              (const pdf_api::SaveToDriveProgress& progress),
              (override));
};

class SaveToDriveEventDispatcherBrowserTest
    : public base::test::WithFeatureOverride,
      public PDFExtensionTestBase {
 public:
  SaveToDriveEventDispatcherBrowserTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  SaveToDriveEventDispatcherBrowserTest(
      const SaveToDriveEventDispatcherBrowserTest&) = delete;
  SaveToDriveEventDispatcherBrowserTest& operator=(
      const SaveToDriveEventDispatcherBrowserTest&) = delete;

  ~SaveToDriveEventDispatcherBrowserTest() override = default;

  bool UseOopif() const override { return GetParam(); }

  void SetUpOnMainThread() override {
    PDFExtensionTestBase::SetUpOnMainThread();

    GURL page_url = chrome_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("test.pdf")));
    auto* extension_frame = LoadPdfGetExtensionHost(page_url);
    ASSERT_TRUE(extension_frame);

    auto time_remaining_calculator =
        std::make_unique<StrictMock<MockTimeRemainingCalculator>>();
    time_remaining_calculator_ = time_remaining_calculator.get();

    auto save_to_drive_recorder =
        std::make_unique<StrictMock<MockSaveToDriveRecorder>>();
    save_to_drive_recorder_ = save_to_drive_recorder.get();

    dispatcher_ = SaveToDriveEventDispatcher::CreateForTesting(
        extension_frame, std::move(time_remaining_calculator),
        std::move(save_to_drive_recorder));
    ASSERT_TRUE(dispatcher_);
  }

  void TearDownOnMainThread() override {
    // At the end of a Save to Drive upload test, send a completed progress to
    // disable the beforeunload dialog in the UI.
    pdf_api::SaveToDriveProgress progress;
    progress.status = pdf_api::SaveToDriveStatus::kUploadCompleted;
    progress.error_type = pdf_api::SaveToDriveErrorType::kNoError;
    progress.uploaded_bytes = 100;
    progress.file_size_bytes = 100;
    EXPECT_CALL(*save_to_drive_recorder_, Record);
    dispatcher_->Notify(std::move(progress));

    save_to_drive_recorder_ = nullptr;
    time_remaining_calculator_ = nullptr;
    dispatcher_.reset();
    PDFExtensionTestBase::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<SaveToDriveEventDispatcher> dispatcher_;
  raw_ptr<StrictMock<MockTimeRemainingCalculator>> time_remaining_calculator_;
  raw_ptr<StrictMock<MockSaveToDriveRecorder>> save_to_drive_recorder_;
};

IN_PROC_BROWSER_TEST_P(SaveToDriveEventDispatcherBrowserTest, Notify) {
  EXPECT_CALL(*time_remaining_calculator_, CalculateTimeRemainingText);
  EXPECT_CALL(*save_to_drive_recorder_, Record);

  auto create_progress = []() {
    pdf_api::SaveToDriveProgress progress;
    progress.status = pdf_api::SaveToDriveStatus::kUploadStarted;
    progress.error_type = pdf_api::SaveToDriveErrorType::kNoError;
    progress.uploaded_bytes = 50;
    progress.file_size_bytes = 100;
    progress.account_email = "test@mail.com";
    progress.account_is_managed = false;
    return progress;
  };

  auto* event_router = extensions::EventRouter::Get(profile());
  extensions::TestEventRouterObserver observer(event_router);

  dispatcher_->Notify(create_progress());

  ASSERT_EQ(observer.events().size(), 1u);
  EXPECT_EQ(observer.events().begin()->first,
            pdf_api::OnSaveToDriveProgress::kEventName);

  pdf_api::SaveToDriveProgress expected_progress = create_progress();
  expected_progress.file_metadata = "50/100 B";

  extensions::Event* captured_event = observer.events().begin()->second.get();
  ASSERT_TRUE(captured_event);
  EXPECT_EQ(captured_event->event_name,
            pdf_api::OnSaveToDriveProgress::kEventName);

  ASSERT_EQ(captured_event->event_args.size(), 2u);
  // The stream URL is not deterministic, so just check that it's a string and
  // not empty.
  ASSERT_TRUE(captured_event->event_args[0].is_string());
  EXPECT_FALSE(captured_event->event_args[0].GetString().empty());
  EXPECT_EQ(captured_event->event_args[1], expected_progress.ToValue());
}

IN_PROC_BROWSER_TEST_P(SaveToDriveEventDispatcherBrowserTest,
                       GetFileMetadataStringForUploadInProgress) {
  EXPECT_CALL(*time_remaining_calculator_, CalculateTimeRemainingText)
      .WillOnce(testing::Return(u"PLACEHOLDER"));
  EXPECT_CALL(*save_to_drive_recorder_, Record);
  pdf_api::SaveToDriveProgress progress;
  progress.status = pdf_api::SaveToDriveStatus::kUploadInProgress;
  progress.error_type = pdf_api::SaveToDriveErrorType::kNoError;
  progress.uploaded_bytes = 50 * 1024 * 1024;
  progress.file_size_bytes = 100 * 1024 * 1024;

  auto* event_router = extensions::EventRouter::Get(profile());
  extensions::TestEventRouterObserver observer(event_router);

  dispatcher_->Notify(std::move(progress));

  extensions::Event* captured_event = observer.events().begin()->second.get();
  ASSERT_TRUE(captured_event);
  std::optional<pdf_api::SaveToDriveProgress> captured_progress =
      pdf_api::SaveToDriveProgress::FromValue(captured_event->event_args[1]);
  ASSERT_TRUE(captured_progress.has_value());
  EXPECT_EQ(*captured_progress->file_metadata, "50.0/100 MB • PLACEHOLDER");
}

IN_PROC_BROWSER_TEST_P(SaveToDriveEventDispatcherBrowserTest,
                       GetFileMetadataStringForUploadCompleted) {
  EXPECT_CALL(*save_to_drive_recorder_, Record);
  pdf_api::SaveToDriveProgress progress;
  progress.status = pdf_api::SaveToDriveStatus::kUploadCompleted;
  progress.error_type = pdf_api::SaveToDriveErrorType::kNoError;
  progress.uploaded_bytes = 50 * 1024 * 1024;
  progress.file_size_bytes = 100 * 1024 * 1024;

  auto* event_router = extensions::EventRouter::Get(profile());
  extensions::TestEventRouterObserver observer(event_router);

  dispatcher_->Notify(std::move(progress));

  extensions::Event* captured_event = observer.events().begin()->second.get();
  ASSERT_TRUE(captured_event);
  std::optional<pdf_api::SaveToDriveProgress> captured_progress =
      pdf_api::SaveToDriveProgress::FromValue(captured_event->event_args[1]);
  ASSERT_TRUE(captured_progress.has_value());
  EXPECT_EQ(*captured_progress->file_metadata, "100 MB • Done");
}

IN_PROC_BROWSER_TEST_P(SaveToDriveEventDispatcherBrowserTest,
                       GetFileMetadataStringForUploadNotStarted) {
  EXPECT_CALL(*save_to_drive_recorder_, Record);
  pdf_api::SaveToDriveProgress progress;
  progress.status = pdf_api::SaveToDriveStatus::kNotStarted;
  progress.error_type = pdf_api::SaveToDriveErrorType::kNoError;

  auto* event_router = extensions::EventRouter::Get(profile());
  extensions::TestEventRouterObserver observer(event_router);

  dispatcher_->Notify(std::move(progress));

  extensions::Event* captured_event = observer.events().begin()->second.get();
  ASSERT_TRUE(captured_event);
  std::optional<pdf_api::SaveToDriveProgress> captured_progress =
      pdf_api::SaveToDriveProgress::FromValue(captured_event->event_args[1]);
  ASSERT_TRUE(captured_progress.has_value());
  EXPECT_FALSE(captured_progress->file_metadata.has_value());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SaveToDriveEventDispatcherBrowserTest);

}  // namespace save_to_drive
