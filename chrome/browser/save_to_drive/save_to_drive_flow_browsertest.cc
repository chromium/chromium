// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_flow.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/save_to_drive/drive_uploader.h"
#include "chrome/browser/save_to_drive/multipart_drive_uploader.h"
#include "chrome/browser/save_to_drive/resumable_drive_uploader.h"
#include "chrome/browser/save_to_drive/save_to_drive_event_dispatcher.h"
#include "chrome/browser/save_to_drive/save_to_drive_recorder.h"
#include "chrome/browser/save_to_drive/time_remaining_calculator.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/save_to_drive/get_account.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "pdf/pdf_features.h"
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

AccountInfo CreateAccountInfo(bool is_managed) {
  AccountInfo::Builder builder(GaiaId("123456789"), "test@mail.com");
  builder.SetAccountId(CoreAccountId::FromGaiaId(GaiaId("123456789")));
  if (is_managed) {
    builder.SetHostedDomain("mail.com");
  }
  return builder.Build();
}

}  // namespace

class MockAccountChooser : public AccountChooser {
 public:
  MOCK_METHOD(void,
              GetAccount,
              (content::WebContents * web_contents,
               base::OnceCallback<void(std::optional<AccountInfo>)>
                   on_account_selected_callback),
              (override));
};

class MockSaveToDriveEventDispatcher : public SaveToDriveEventDispatcher {
 public:
  MockSaveToDriveEventDispatcher(
      content::RenderFrameHost* render_frame_host,
      const GURL& stream_url,
      std::unique_ptr<TimeRemainingCalculator> time_remaining_calculator)
      : SaveToDriveEventDispatcher(render_frame_host,
                                   stream_url,
                                   std::move(time_remaining_calculator),
                                   nullptr) {}
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

class SaveToDriveFlowBrowserTest : public base::test::WithFeatureOverride,
                                   public PDFExtensionTestBase {
 public:
  SaveToDriveFlowBrowserTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}
  SaveToDriveFlowBrowserTest(const SaveToDriveFlowBrowserTest&) = delete;
  SaveToDriveFlowBrowserTest& operator=(const SaveToDriveFlowBrowserTest&) =
      delete;
  ~SaveToDriveFlowBrowserTest() override = default;

  bool UseOopif() const override { return GetParam(); }

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        PDFExtensionTestBase::GetEnabledFeatures();
    enabled_features.push_back(
        {chrome_pdf::features::kPdfSaveToDriveSurvey,
         {{"enterprise-trigger-id", "EnterpriseTriggerId"},
          {"consumer-trigger-id", "ConsumerTriggerId"}}});
    return enabled_features;
  }

  void SetUpOnMainThread() override {
    PDFExtensionTestBase::SetUpOnMainThread();

    GURL page_url = chrome_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("test.pdf")));
    auto* rfh = LoadPdfGetExtensionHost(page_url);
    ASSERT_TRUE(rfh);

    auto event_dispatcher =
        std::make_unique<testing::StrictMock<MockSaveToDriveEventDispatcher>>(
            rfh, GURL("https://example.com/stream"),
            std::make_unique<TimeRemainingCalculator>());
    auto content_reader =
        std::make_unique<testing::StrictMock<MockContentReader>>();
    auto account_chooser =
        std::make_unique<testing::StrictMock<MockAccountChooser>>();
    account_chooser_ = account_chooser.get();
    hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));

    SaveToDriveFlow::CreateForCurrentDocument(
        rfh, std::move(event_dispatcher), std::move(content_reader),
        std::move(account_chooser), hats_service_);
    test_api_ = std::make_unique<SaveToDriveFlow::TestApi>(
        SaveToDriveFlow::GetForCurrentDocument(rfh));
  }

  void TearDownOnMainThread() override {
    account_chooser_ = nullptr;
    hats_service_ = nullptr;
    test_api_.reset();
    PDFExtensionTestBase::TearDownOnMainThread();
  }

  const MockSaveToDriveEventDispatcher& event_dispatcher() const {
    return *static_cast<const MockSaveToDriveEventDispatcher*>(
        test_api_->event_dispatcher());
  }

  const MockContentReader& content_reader() const {
    return *static_cast<const MockContentReader*>(test_api_->content_reader());
  }

  content::RenderFrameHost* rfh() { return test_api_->rfh(); }

  void SimulateAccountChooserAction(std::optional<AccountInfo> account_info) {
    EXPECT_CALL(*account_chooser_, GetAccount)
        .WillOnce(
            [account_info = std::move(account_info), this](
                content::WebContents* web_contents,
                base::OnceCallback<void(std::optional<AccountInfo>)> callback) {
              // The callback could kill the flow, which would destroy the
              // account chooser. It needs to be reset to avoid a dangling
              // pointer.
              account_chooser_ = nullptr;
              std::move(callback).Run(std::move(account_info));
            });
  }

  void TestCreateMultipartUploaderForSmallFile(bool is_managed) {
    SimulateAccountChooserAction(CreateAccountInfo(is_managed));
    EXPECT_CALL(event_dispatcher(),
                Notify(AllOf(Field(&SaveToDriveProgress::status,
                                   SaveToDriveStatus::kInitiated),
                             Field(&SaveToDriveProgress::error_type,
                                   SaveToDriveErrorType::kNoError))));
    EXPECT_CALL(
        event_dispatcher(),
        Notify(AllOf(
            Field(&SaveToDriveProgress::status,
                  SaveToDriveStatus::kAccountSelected),
            Field(&SaveToDriveProgress::error_type,
                  SaveToDriveErrorType::kNoError),
            Field(&SaveToDriveProgress::account_email, "test@mail.com"),
            Field(&SaveToDriveProgress::account_is_managed, is_managed))));

    // Since IdentityManager is not set up, the OAuth fetch will fail.
    EXPECT_CALL(
        event_dispatcher(),
        Notify(
            AllOf(Field(&SaveToDriveProgress::status,
                        SaveToDriveStatus::kUploadFailed),
                  Field(&SaveToDriveProgress::error_type,
                        SaveToDriveErrorType::kOauthError),
                  Field(&SaveToDriveProgress::account_email, "test@mail.com"),
                  Field(&SaveToDriveProgress::account_is_managed, is_managed))))
        .WillOnce([&]() {
          auto* drive_uploader = test_api_->drive_uploader();
          ASSERT_TRUE(drive_uploader);
          EXPECT_EQ(drive_uploader->get_drive_uploader_type(),
                    DriveUploaderType::kMultipart);
        });
    EXPECT_CALL(content_reader(), Open)
        .WillOnce(base::test::RunOnceCallback<0>(/*success=*/true));
    EXPECT_CALL(content_reader(), GetSize).WillOnce(Return(100));
    SaveToDriveFlow::GetForCurrentDocument(rfh())->Run();
  }

 protected:
  std::unique_ptr<SaveToDriveFlow::TestApi> test_api_;
  raw_ptr<MockAccountChooser> account_chooser_ = nullptr;
  raw_ptr<MockHatsService> hats_service_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(SaveToDriveFlowBrowserTest, AccountChooserCanceled) {
  SimulateAccountChooserAction(std::nullopt);
  EXPECT_CALL(event_dispatcher(),
              Notify(AllOf(Field(&SaveToDriveProgress::status,
                                 SaveToDriveStatus::kInitiated),
                           Field(&SaveToDriveProgress::error_type,
                                 SaveToDriveErrorType::kNoError))));
  EXPECT_CALL(
      event_dispatcher(),
      Notify(AllOf(
          Field(&SaveToDriveProgress::status, SaveToDriveStatus::kUploadFailed),
          Field(&SaveToDriveProgress::error_type,
                SaveToDriveErrorType::kAccountChooserCanceled))));
  SaveToDriveFlow::GetForCurrentDocument(rfh())->Run();
}

IN_PROC_BROWSER_TEST_P(SaveToDriveFlowBrowserTest, ContentReadFails) {
  AccountInfo account_info = CreateAccountInfo(/*is_managed=*/false);
  account_info =
      AccountInfo::Builder(account_info).SetHostedDomain("example.com").Build();
  SimulateAccountChooserAction(std::move(account_info));
  EXPECT_CALL(event_dispatcher(),
              Notify(AllOf(Field(&SaveToDriveProgress::status,
                                 SaveToDriveStatus::kInitiated),
                           Field(&SaveToDriveProgress::error_type,
                                 SaveToDriveErrorType::kNoError))));
  EXPECT_CALL(
      event_dispatcher(),
      Notify(AllOf(Field(&SaveToDriveProgress::status,
                         SaveToDriveStatus::kAccountSelected),
                   Field(&SaveToDriveProgress::error_type,
                         SaveToDriveErrorType::kNoError),
                   Field(&SaveToDriveProgress::account_email, "test@mail.com"),
                   Field(&SaveToDriveProgress::account_is_managed, true))));

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

IN_PROC_BROWSER_TEST_P(SaveToDriveFlowBrowserTest,
                       CreatesMultipartUploaderForSmallFile) {
  TestCreateMultipartUploaderForSmallFile(/*is_managed=*/false);
}

IN_PROC_BROWSER_TEST_P(SaveToDriveFlowBrowserTest,
                       CreatesResumableUploaderForLargeFile) {
  SimulateAccountChooserAction(CreateAccountInfo(/*is_managed=*/false));
  EXPECT_CALL(event_dispatcher(),
              Notify(AllOf(Field(&SaveToDriveProgress::status,
                                 SaveToDriveStatus::kInitiated),
                           Field(&SaveToDriveProgress::error_type,
                                 SaveToDriveErrorType::kNoError))));
  EXPECT_CALL(
      event_dispatcher(),
      Notify(AllOf(Field(&SaveToDriveProgress::status,
                         SaveToDriveStatus::kAccountSelected),
                   Field(&SaveToDriveProgress::error_type,
                         SaveToDriveErrorType::kNoError),
                   Field(&SaveToDriveProgress::account_email, "test@mail.com"),
                   Field(&SaveToDriveProgress::account_is_managed, false))));
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
        EXPECT_EQ(drive_uploader->get_drive_uploader_type(),
                  DriveUploaderType::kResumable);
      });
  EXPECT_CALL(content_reader(), Open)
      .WillOnce(base::test::RunOnceCallback<0>(/*success=*/true));
  // 5 MB + 1 byte to ensure it's greater than the threshold.
  EXPECT_CALL(content_reader(), GetSize).WillOnce(Return(5 * 1024 * 1024 + 1));
  SaveToDriveFlow::GetForCurrentDocument(rfh())->Run();
}

IN_PROC_BROWSER_TEST_P(SaveToDriveFlowBrowserTest,
                       ShowConsumerSurveyAfterUpload) {
  EXPECT_CALL(
      *hats_service_,
      LaunchDelayedSurvey(kHatsSurveyConsumerTriggerPdfSaveToDrive, _, _, _));
  TestCreateMultipartUploaderForSmallFile(/*is_managed=*/false);
}

IN_PROC_BROWSER_TEST_P(SaveToDriveFlowBrowserTest,
                       ShowEnterpriseSurveyAfterUpload) {
  EXPECT_CALL(
      *hats_service_,
      LaunchDelayedSurvey(kHatsSurveyEnterpriseTriggerPdfSaveToDrive, _, _, _));
  TestCreateMultipartUploaderForSmallFile(/*is_managed=*/true);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SaveToDriveFlowBrowserTest);

}  // namespace save_to_drive
