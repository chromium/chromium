// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_flow.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/save_to_drive/drive_uploader.h"
#include "chrome/browser/save_to_drive/multipart_drive_uploader.h"
#include "chrome/browser/save_to_drive/resumable_drive_uploader.h"
#include "chrome/browser/save_to_drive/save_to_drive_event_dispatcher.h"
#include "chrome/browser/save_to_drive/time_remaining_calculator.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;
using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Return;

namespace save_to_drive {
namespace {

AccountInfo CreateAccountInfo() {
  AccountInfo account_info;
  account_info.email = "test@mail.com";
  account_info.gaia = GaiaId("1234567890");
  account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
  return account_info;
}

}  // namespace

class MockSaveToDriveEventDispatcher : public SaveToDriveEventDispatcher {
 public:
  MockSaveToDriveEventDispatcher(
      content::RenderFrameHost* render_frame_host,
      const GURL& stream_url,
      std::unique_ptr<TimeRemainingCalculator> time_remaining_calculator)
      : SaveToDriveEventDispatcher(render_frame_host,
                                   stream_url,
                                   std::move(time_remaining_calculator)) {}
  ~MockSaveToDriveEventDispatcher() override = default;

  MOCK_METHOD(void, Notify, (SaveToDriveProgress progress), (const, override));
};

class MockContentReader : public ContentReader {
 public:
  MOCK_METHOD(void, Open, (OpenCallback callback), (override));
  MOCK_METHOD(size_t, GetSize, (), (override));
  MOCK_METHOD(void,
              Read,
              (uint32_t offset, uint32_t size, ContentReadCallback callback),
              (override));
  MOCK_METHOD(void, Close, (), (override));
};

class SaveToDriveFlowTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    auto event_dispatcher =
        std::make_unique<testing::StrictMock<MockSaveToDriveEventDispatcher>>(
            rfh(), GURL("https://example.com/stream"),
            std::make_unique<TimeRemainingCalculator>());
    auto content_reader =
        std::make_unique<testing::StrictMock<MockContentReader>>();

    SaveToDriveFlow::CreateForCurrentDocument(
        rfh(), std::move(event_dispatcher), std::move(content_reader));
    test_api_ = std::make_unique<SaveToDriveFlow::TestApi>(
        SaveToDriveFlow::GetForCurrentDocument(rfh()));
  }

  const MockSaveToDriveEventDispatcher& event_dispatcher() const {
    return *static_cast<const MockSaveToDriveEventDispatcher*>(
        test_api_->event_dispatcher());
  }

  const MockContentReader& content_reader() const {
    return *static_cast<const MockContentReader*>(test_api_->content_reader());
  }

  content::RenderFrameHost* rfh() const {
    return web_contents_->GetPrimaryMainFrame();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<SaveToDriveFlow::TestApi> test_api_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

TEST_F(SaveToDriveFlowTest, AccountChooserCanceled) {
  test_api_->SimulateAccountChooserAction(std::nullopt);
  EXPECT_CALL(event_dispatcher(),
              Notify(AllOf(Field(&SaveToDriveProgress::status,
                                 SaveToDriveStatus::kInitiated))));
  EXPECT_CALL(
      event_dispatcher(),
      Notify(AllOf(
          Field(&SaveToDriveProgress::status, SaveToDriveStatus::kUploadFailed),
          Field(&SaveToDriveProgress::error_type,
                SaveToDriveErrorType::kAccountChooserCanceled))));
  SaveToDriveFlow::GetForCurrentDocument(rfh())->Run();
}

TEST_F(SaveToDriveFlowTest, ContentReadFails) {
  AccountInfo account_info = CreateAccountInfo();
  account_info.hosted_domain = "example.com";
  test_api_->SimulateAccountChooserAction(std::move(account_info));
  EXPECT_CALL(event_dispatcher(),
              Notify(AllOf(Field(&SaveToDriveProgress::status,
                                 SaveToDriveStatus::kInitiated))));
  EXPECT_CALL(content_reader(), Open)
      .WillOnce(base::test::RunOnceCallback<0>(/*success=*/false));
  EXPECT_CALL(content_reader(), Read).Times(0);
  EXPECT_CALL(
      event_dispatcher(),
      Notify(AllOf(
          Field(&SaveToDriveProgress::status, SaveToDriveStatus::kUploadFailed),
          Field(&SaveToDriveProgress::error_type,
                SaveToDriveErrorType::kUnknownError),
          Field(&SaveToDriveProgress::account_email, "test@mail.com"),
          Field(&SaveToDriveProgress::account_is_managed, true))));
  SaveToDriveFlow::GetForCurrentDocument(rfh())->Run();
}

TEST_F(SaveToDriveFlowTest, CreatesMultipartUploaderForSmallFile) {
  test_api_->SimulateAccountChooserAction(CreateAccountInfo());
  EXPECT_CALL(event_dispatcher(),
              Notify(AllOf(Field(&SaveToDriveProgress::status,
                                 SaveToDriveStatus::kInitiated))));
  // Since IdentityManager is not set up, the OAuth fetch will fail.
  EXPECT_CALL(
      event_dispatcher(),
      Notify(AllOf(
          Field(&SaveToDriveProgress::status, SaveToDriveStatus::kUploadFailed),
          Field(&SaveToDriveProgress::error_type,
                SaveToDriveErrorType::kOauthError),
          Field(&SaveToDriveProgress::account_email, "test@mail.com"),
          Field(&SaveToDriveProgress::account_is_managed, false))))
      .WillOnce([&]() {
        auto* drive_uploader = test_api_->drive_uploader();
        ASSERT_TRUE(drive_uploader);
        EXPECT_EQ(drive_uploader->get_drive_uploader_type_for_testing(),
                  DriveUploaderType::kMultipart);
      });
  EXPECT_CALL(content_reader(), Open)
      .WillOnce(base::test::RunOnceCallback<0>(/*success=*/true));
  EXPECT_CALL(content_reader(), GetSize).WillOnce(Return(100));
  SaveToDriveFlow::GetForCurrentDocument(rfh())->Run();
}

TEST_F(SaveToDriveFlowTest, CreatesResumableUploaderForLargeFile) {
  test_api_->SimulateAccountChooserAction(CreateAccountInfo());
  EXPECT_CALL(event_dispatcher(),
              Notify(AllOf(Field(&SaveToDriveProgress::status,
                                 SaveToDriveStatus::kInitiated))));
  // Since IdentityManager is not set up, the OAuth fetch will fail.
  EXPECT_CALL(
      event_dispatcher(),
      Notify(AllOf(
          Field(&SaveToDriveProgress::status, SaveToDriveStatus::kUploadFailed),
          Field(&SaveToDriveProgress::error_type,
                SaveToDriveErrorType::kOauthError),
          Field(&SaveToDriveProgress::account_email, "test@mail.com"),
          Field(&SaveToDriveProgress::account_is_managed, false))))
      .WillOnce([&]() {
        auto* drive_uploader = test_api_->drive_uploader();
        ASSERT_TRUE(drive_uploader);
        EXPECT_EQ(drive_uploader->get_drive_uploader_type_for_testing(),
                  DriveUploaderType::kResumable);
      });
  EXPECT_CALL(content_reader(), Open)
      .WillOnce(base::test::RunOnceCallback<0>(/*success=*/true));
  // 5 MB + 1 byte to ensure it's greater than the threshold.
  EXPECT_CALL(content_reader(), GetSize).WillOnce(Return(5 * 1024 * 1024 + 1));
  SaveToDriveFlow::GetForCurrentDocument(rfh())->Run();
}

}  // namespace save_to_drive
