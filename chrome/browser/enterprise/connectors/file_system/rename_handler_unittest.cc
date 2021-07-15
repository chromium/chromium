// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"
#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using download::DownloadInterruptReason;
using testing::_;
using testing::Invoke;
using testing::Return;

namespace enterprise_connectors {

const char kBox[] = "box";
constexpr char kWildcardSendDownloadToCloudPref[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [
      {
        "url_list": ["*"],
        "mime_types": ["text/plain", "image/png", "application/zip"]
      }
    ]
  }
])";

constexpr char kRenameSendDownloadToCloudPref[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [
      {
        "url_list": ["renameme.com"],
        "mime_types": ["text/plain", "image/png", "application/zip"]
      }
    ]
  }
])";

using RenameHandler = FileSystemRenameHandler;

class RenameHandlerCreateTestBase : public testing::Test {
 protected:
  Profile* profile() { return profile_; }

  void Init(bool enable_feature_flag, const char* pref_value) {
    if (enable_feature_flag) {
      scoped_feature_list_.InitWithFeatures({kFileSystemConnectorEnabled}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {kFileSystemConnectorEnabled});
    }

    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    // Set a dummy policy value to enable the rename handler.
    profile_->GetPrefs()->Set(
        ConnectorPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
        *base::JSONReader::Read(pref_value));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  TestingProfile* profile_;
};

class RenameHandlerCreateTest : public RenameHandlerCreateTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  RenameHandlerCreateTest() {
    Init(enable_feature_flag(), kWildcardSendDownloadToCloudPref);
  }

  bool enable_feature_flag() const { return GetParam(); }
};

TEST_P(RenameHandlerCreateTest, FeatureFlagTest) {
  // Check the precondition regardless of download item: feature is enabled.
  auto settings = GetFileSystemSettings(profile());
  ASSERT_EQ(enable_feature_flag(), settings.has_value());

  // Ensure a RenameHandler can be created with the profile in download item.
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://renameme.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
  auto handler = RenameHandler::CreateIfNeeded(&item);
  ASSERT_EQ(enable_feature_flag(), handler.get() != nullptr);
}

TEST_P(RenameHandlerCreateTest, Completed_LoadFromRerouteInfo) {
  DownloadItemForTest test_item_(FILE_PATH_LITERAL("handler_loaded_info.txt"));
  test_item_.SetState(DownloadItemForTest::DownloadState::COMPLETE);

  DownloadItemRerouteInfo rerouted_info;
  rerouted_info.set_service_provider(BoxUploader::kServiceProvider);
  rerouted_info.mutable_box()->set_file_id("12345");
  test_item_.SetRerouteInfo(rerouted_info);

  // Handler should be created regardless of settings/policies since item upload
  // was already completed.
  auto handler = RenameHandler::CreateIfNeeded(&test_item_);
  ASSERT_TRUE(handler.get());
}

TEST_P(RenameHandlerCreateTest, Completed_NoRerouteInfo) {
  DownloadItemForTest test_item_(FILE_PATH_LITERAL("handler_loaded_info.txt"));
  test_item_.SetState(DownloadItemForTest::DownloadState::COMPLETE);

  // Handler should NOT be created regardless of settings/policies since item
  // download was already completed without upload previously.
  auto handler = RenameHandler::CreateIfNeeded(&test_item_);
  ASSERT_FALSE(handler.get());
}

INSTANTIATE_TEST_CASE_P(, RenameHandlerCreateTest, testing::Bool());

class RenameHandlerCreateTest_ByUrl : public RenameHandlerCreateTestBase {
 public:
  RenameHandlerCreateTest_ByUrl() {
    Init(true, kRenameSendDownloadToCloudPref);
  }
};

TEST_F(RenameHandlerCreateTest_ByUrl, NoUrlMatchesPattern) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://one.com/file.txt"));
  item.SetTabUrl(GURL("https://two.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = RenameHandler::CreateIfNeeded(&item);
  ASSERT_EQ(nullptr, handler.get());
}

TEST_F(RenameHandlerCreateTest_ByUrl, FileUrlMatchesPattern) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://renameme.com/file.txt"));
  item.SetTabUrl(GURL("https://two.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = RenameHandler::CreateIfNeeded(&item);
  ASSERT_NE(nullptr, handler.get());
}

TEST_F(RenameHandlerCreateTest_ByUrl, TabUrlMatchesPattern) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://one.com/file.txt"));
  item.SetTabUrl(GURL("https://renameme.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = RenameHandler::CreateIfNeeded(&item);
  ASSERT_NE(nullptr, handler.get());
}

TEST_F(RenameHandlerCreateTest_ByUrl, DisallowByMimeType) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://renameme.com/file.json"));
  item.SetTabUrl(GURL("https://renameme.com"));
  item.SetMimeType("application/json");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = RenameHandler::CreateIfNeeded(&item);
  ASSERT_EQ(nullptr, handler.get());
}

class RenameHandlerCreateTest_Wildcard : public RenameHandlerCreateTestBase {
 public:
  RenameHandlerCreateTest_Wildcard() {
    Init(true, kWildcardSendDownloadToCloudPref);
  }
};

TEST_F(RenameHandlerCreateTest_Wildcard, AllowedByWildcard) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://one.com/file.txt"));
  item.SetTabUrl(GURL("https://two.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = RenameHandler::CreateIfNeeded(&item);
  ASSERT_NE(nullptr, handler.get());
}

constexpr char ATokenBySignIn[] = "ATokenBySignIn";
constexpr char RTokenBySignIn[] = "RTokenBySignIn";
constexpr char ATokenByFetcher[] = "ATokenByFetcher";
constexpr char RTokenForFetcher[] = "RTokenForFetcher";

class RenameHandlerForTest : public FileSystemRenameHandler {
 public:
  using FileSystemRenameHandler::FileSystemRenameHandler;
  using FileSystemRenameHandler::OpenDownload;
  using FileSystemRenameHandler::SetUploaderForTesting;
  using FileSystemRenameHandler::ShowDownloadInContext;
  using AuthErr = GoogleServiceAuthError;

  MOCK_METHOD(void,
              PromptUserSignInForAuthorization,
              (content::WebContents * contents),
              (override));

  void ReturnSignInSuccess() {
    OnAuthorization(AuthErr::AuthErrorNone(), ATokenBySignIn, RTokenBySignIn);
  }

  void ReturnSignInFailure() {
    const auto fail_reason = AuthErr::FromInvalidGaiaCredentialsReason(
        AuthErr::InvalidGaiaCredentialsReason::CREDENTIALS_REJECTED_BY_SERVER);
    OnAuthorization(fail_reason, std::string(), std::string());
  }

  void ReturnSignInCancellation() {
    OnAuthorization(AuthErr(AuthErr::State::REQUEST_CANCELED), std::string(),
                    std::string());
  }

  MOCK_METHOD(void,
              FetchAccessToken,
              (content::BrowserContext * context,
               const std::string& refresh_token),
              (override));

  void ReturnFetchSuccess() {
    OnAccessTokenFetched(AuthErr::AuthErrorNone(), ATokenByFetcher,
                         RTokenForFetcher);
  }

  void ReturnFetchFailure() {
    const auto fail_reason = AuthErr::FromInvalidGaiaCredentialsReason(
        AuthErr::InvalidGaiaCredentialsReason::CREDENTIALS_REJECTED_BY_SERVER);
    OnAccessTokenFetched(fail_reason, std::string(), std::string());
  }

  void ReturnFetchNetworkError() {
    const auto fail_reason = AuthErr::FromConnectionError(net::ERR_TIMED_OUT);
    OnAccessTokenFetched(fail_reason, std::string(), std::string());
  }
};

const base::FilePath kTargetFileName(FILE_PATH_LITERAL("rename_handler.txt"));
const char kUploadedFileId[] = "314159";  // Should match below.
const char kUploadedFileUrl[] = "https://example.com/file/314159";
const char kDestinationFolderUrl[] = "https://example.com/folder/1337";

class MockUploader : public BoxUploader {
 public:
  explicit MockUploader(download::DownloadItem* download_item)
      : BoxUploader(download_item) {}

  MOCK_METHOD(GURL, GetUploadedFileUrl, (), (const override));
  MOCK_METHOD(GURL, GetDestinationFolderUrl, (), (const override));

  MOCK_METHOD(
      void,
      TryTask,
      (scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
       const std::string& access_token),
      (override));

  MOCK_METHOD(void, StartCurrentApiCall, (), (override));

  std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() override {
    return std::make_unique<MockApiCallFlow>();
  }

  void TryTaskSuccess() {
    // For invoking progress update in StartUpload().
    expected_reason_ = InterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE;
    EXPECT_CALL(*this, StartCurrentApiCall()).WillOnce(Invoke([this]() {
      SetUploadApiCallFlowDoneForTesting(expected_reason_, kUploadedFileId);
    }));

    EXPECT_CALL(*this, GetUploadedFileUrl())
        .WillOnce(Return(GURL(kUploadedFileUrl)));

    StartUpload();
  }

  void TryTaskFailure() {
    EXPECT_CALL(*this, StartCurrentApiCall()).Times(0);
    EXPECT_CALL(*this, GetUploadedFileUrl()).WillOnce(Return(GURL()));
    expected_reason_ = InterruptReason::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
    SetUploadApiCallFlowDoneForTesting(expected_reason_, {});
  }

  void ExpectNoTryTask(InterruptReason reason) {
    expected_reason_ = reason;
    EXPECT_CALL(*this, GetUploadedFileUrl()).WillOnce(Return(GURL()));
    EXPECT_CALL(*this, TryTask(_, _)).Times(0);
  }

  InterruptReason expected_reason_ =
      InterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE;
};

class RenameHandlerTestBase {
 protected:
  RenameHandlerForTest* handler() { return handler_.get(); }
  MockUploader* uploader() { return uploader_; }

  void SetUp(TestingProfile* profile) {
    feature_list_.InitWithFeatures({kFileSystemConnectorEnabled}, {});

    // Make sure that from the connectors manager point of view the file system
    // connector should be enabled.  So that the only thing that controls
    // whether the rename handler is used or not is the feature flag.
    profile->GetPrefs()->Set(
        ConnectorPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
        *base::JSONReader::Read(kWildcardSendDownloadToCloudPref));
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);

    item_.SetTargetFilePath(kTargetFileName);
    item_.SetURL(GURL("https://any.com"));
    content::DownloadItemUtils::AttachInfo(&item_, profile,
                                           web_contents_.get());
    ASSERT_TRUE(base::WriteFile(item_.GetFullPath(), "RenameHandlerTest"))
        << "Failed to create " << item_.GetFullPath();

    handler_ = CreateHandler();
  }

  virtual std::unique_ptr<RenameHandlerForTest> CreateHandler() {
    ConnectorsService* service = ConnectorsServiceFactory::GetForBrowserContext(
        content::DownloadItemUtils::GetBrowserContext(&item_));
    auto settings = service->GetFileSystemSettings(
        item_.GetURL(), FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
    settings->service_provider = kBox;
    auto handler = std::make_unique<RenameHandlerForTest>(
        &item_, std::move(settings.value()));

    auto uploader = std::make_unique<MockUploader>(&item_);
    uploader_ = uploader.get();
    handler->SetUploaderForTesting(std::move(uploader));
    return handler;
  }

  void TearDown() {
    handler_.reset();
    web_contents_.reset();
    ASSERT_FALSE(base::PathExists(item_.GetFullPath()) &&
                 base::PathExists(item_.GetTargetFilePath()))
        << "File should have been deleted regardless of rerouting success or "
           "failure";
  }

  DownloadItemForTest item_{FILE_PATH_LITERAL("rename_handler_test.txt")};

 private:
  std::unique_ptr<RenameHandlerForTest> handler_;
  MockUploader* uploader_;

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
};

class RenameHandlerOAuth2Test : public testing::Test,
                                public RenameHandlerTestBase {
 public:
  RenameHandlerOAuth2Test()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetUp() override {
    OSCryptMocker::SetUp();
    RenameHandlerTestBase::SetUp(profile_);
    EXPECT_CALL(*uploader(), GetDestinationFolderUrl()).Times(0);
  }

  void TearDown() override {
    ASSERT_EQ(download_cb_reason_, uploader()->expected_reason_);
    RenameHandlerTestBase::TearDown();
    OSCryptMocker::TearDown();
  }

 protected:
  void RunHandler() {
    run_loop_ = std::make_unique<base::RunLoop>();
    download::DownloadItemRenameHandler* download_handler_ = handler();
    download_handler_->Start(
        base::BindRepeating(&RenameHandlerOAuth2Test::OnProgressUpdate,
                            base::Unretained(this)),
        base::BindOnce(&RenameHandlerOAuth2Test::OnUploadComplete,
                       base::Unretained(this)));
    run_loop_->Run();
  }

  void VerifyBothTokensClear() {
    std::string atoken, rtoken;
    ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken))
        << "Access Token: " << atoken << "\nRefresh Token: " << rtoken;
    ASSERT_TRUE(atoken.empty());
    ASSERT_TRUE(rtoken.empty());
  }

  void VerifyBothTokensSetBySignIn() {
    std::string atoken, rtoken;
    ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken))
        << "Access Token: " << atoken << "\nRefresh Token: " << rtoken;
    ASSERT_EQ(atoken, ATokenBySignIn);
    ASSERT_EQ(rtoken, RTokenBySignIn);
  }

  void VerifyBothTokensSetByFetcher() {
    std::string atoken, rtoken;
    ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken))
        << "Access Token: " << atoken << "\nRefresh Token: " << rtoken;
    ASSERT_EQ(atoken, ATokenByFetcher);
    ASSERT_EQ(rtoken, RTokenForFetcher);
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

  int download_cb_count_ = 0;
  DownloadInterruptReason download_cb_reason_;
  GURL uploaded_file_url_;
  base::FilePath uploaded_file_name_;

 private:
  void OnProgressUpdate(
      const download::DownloadItemRenameProgressUpdate& update) {
    uploaded_file_name_ = update.target_file_name;
  }

  void OnUploadComplete(DownloadInterruptReason reason,
                        const base::FilePath& final_name) {
    ++download_cb_count_;
    download_cb_reason_ = reason;
    uploaded_file_url_ = uploader()->GetUploadedFileUrl();
    if (!uploaded_file_name_.empty()) {
      EXPECT_EQ(uploaded_file_name_, final_name);
    }
    run_loop_->Quit();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(RenameHandlerOAuth2Test, NullPtrs) {
  ::testing::InSequence seq;
  constexpr char ATokenPlaceholder[] = "uselessAToken";
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenPlaceholder, RTokenForFetcher);
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  uploader()->ExpectNoTryTask(download::DOWNLOAD_INTERRUPT_REASON_CRASH);

  // Clear the map keyed in |download_item| to test this, since they are not
  // assumed to always be valid on the UI thread.
  item_.ClearAllUserData();

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_FALSE(uploaded_file_url_.is_valid());

  // Verify that the tokens stored are not updated.
  std::string atoken, rtoken;
  ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken));
  ASSERT_EQ(atoken, ATokenPlaceholder);
  ASSERT_EQ(rtoken, RTokenForFetcher);
}

// Test cases are written according to The OAuth2 "Dance" in rename_handler.cc;
// all should be finished via 2a unless aborted via 1b.

////////////////////////////////////////////////////////////////////////////////
// Test cases for (1): Start with no valid token.
////////////////////////////////////////////////////////////////////////////////

// Case 1a->2a: Both tokens will be set; callback returned with success.
TEST_F(RenameHandlerOAuth2Test, SignInSuccessThenUploaderSuccess) {
  ::testing::InSequence seq;
  // 1a: PromptUserSignInForAuthorization() succeeds.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(Invoke(handler(), &RenameHandlerForTest::ReturnSignInSuccess));
  // ->2a: TryUploaderTask() should be called after and succeed.
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskSuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_EQ(uploaded_file_url_, kUploadedFileUrl);
  ASSERT_EQ(uploaded_file_name_, kTargetFileName);
  VerifyBothTokensSetBySignIn();
}

// Case 1b: Both tokens will be clear; callback returned with failure.
TEST_F(RenameHandlerOAuth2Test, SignInCancellationSoAbort) {
  ::testing::InSequence seq;
  // 1b: PromptUserSignInForAuthorization() fails with Cancellation so abort.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(
          Invoke(handler(), &RenameHandlerForTest::ReturnSignInCancellation));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  uploader()->ExpectNoTryTask(
      download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());
  VerifyBothTokensClear();
}

// Case 1c->1a: Retry sign in, but terminate there without going through the
// uploader since 1a->2 is already covered in another test case.
TEST_F(RenameHandlerOAuth2Test, SignInFailureSoRetry) {
  ::testing::InSequence seq;
  // 1c->1a: PromptUserSignInForAuthorization() fails with other reasons than
  // Cancellation so should be called again.
  int authen_cb = 0;
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillRepeatedly(Invoke([this, &authen_cb](content::WebContents*) {
        ++authen_cb;
        if (authen_cb == 1) {
          handler()->ReturnSignInFailure();
        } else if (authen_cb == 2) {
          VerifyBothTokensClear();
          uploader()->TryTaskFailure();
          // Terminate here since 1a->2 is already covered.
        }
        ASSERT_LE(authen_cb, 2) << "Should've terminated above";
      }));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  EXPECT_CALL(*uploader(), TryTask(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(authen_cb, 2);
  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());  // Notified failure to terminate.
  VerifyBothTokensClear();
}

// Case 1c but failed to clear token: Terminate upload flow with internal error.
TEST_F(RenameHandlerOAuth2Test, SignInFailureThenClearTokenFailure) {
  ::testing::InSequence seq;
  // 1c->1a: PromptUserSignInForAuthorization() fails with other reasons than
  // Cancellation so should be called again.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(Invoke([this](content::WebContents*) {
        item_.ClearAllUserData();  // To make GetPrefs() return nullptr.
        handler()->ReturnSignInFailure();
      }));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  uploader()->ExpectNoTryTask(download::DOWNLOAD_INTERRUPT_REASON_CRASH);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());  // Notified failure to terminate.
}

////////////////////////////////////////////////////////////////////////////////
// Test cases for (3): Start with only refresh token.
////////////////////////////////////////////////////////////////////////////////

// Case 3a->2a: Fetch access token with refresh token succeeds, so should
// TryUploaderTask();
TEST_F(RenameHandlerOAuth2Test, FetchAccessTokenSuccess) {
  ::testing::InSequence seq;
  // Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), kBox, std::string(), RTokenForFetcher);
  // 3a.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(handler(), &RenameHandlerForTest::ReturnFetchSuccess));
  // ->2a.
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskSuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_EQ(uploaded_file_url_, kUploadedFileUrl);
  ASSERT_EQ(uploaded_file_name_, kTargetFileName);
  VerifyBothTokensSetByFetcher();
}

// Case 3b->1: Fetch access token with refresh token fails, so should clear
// tokens and PromptUserSignInForAuthorization().
TEST_F(RenameHandlerOAuth2Test, FetchAccessTokenFailureSoPromptForSignIn) {
  ::testing::InSequence seq;
  // Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), kBox, std::string(), RTokenForFetcher);
  // 3b.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(handler(), &RenameHandlerForTest::ReturnFetchFailure));
  // ->1: Prompt user to sign in, but terminate because Case 1 is already
  // covered.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskSuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*uploader(), TryTask(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_EQ(uploaded_file_url_, kUploadedFileUrl);
  ASSERT_EQ(uploaded_file_name_, kTargetFileName);
  VerifyBothTokensClear();
}

// Case 3c: Fetch access token failed with network error, so don't clear the
// tokens, but do abort the upload with the network failure reason.
TEST_F(RenameHandlerOAuth2Test, FetchAccessTokenNetworkFailureSoAbort) {
  ::testing::InSequence seq;
  // 3c: Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), kBox, std::string(), RTokenForFetcher);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(
          Invoke(handler(), &RenameHandlerForTest::ReturnFetchNetworkError));

  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  uploader()->ExpectNoTryTask(
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_FALSE(uploaded_file_url_.is_valid());
  // Verify that the tokens stored are not updated.
  std::string atoken, rtoken;
  ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken));
  ASSERT_TRUE(atoken.empty());
  ASSERT_EQ(rtoken, RTokenForFetcher);
}

////////////////////////////////////////////////////////////////////////////////
// Test cases for (2): Start with both tokens.
////////////////////////////////////////////////////////////////////////////////

// Case 2a(failure): TryUploaderTask() with existing access token and fails,
// but both tokens stay.
TEST_F(RenameHandlerOAuth2Test, StartWithAccessTokenThenUploaderFailure) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenByFetcher, RTokenForFetcher);
  // 2a:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskFailure));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());
  // Verify that uploader failure did not affect stored credentials.
  VerifyBothTokensSetByFetcher();
}

// Case 2a(success): TryUploaderTask() with existing access token and
// succeeds.
TEST_F(RenameHandlerOAuth2Test, StartWithAccessTokenThenUploaderSuccess) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenByFetcher, RTokenForFetcher);
  // 2a:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskSuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_EQ(uploaded_file_url_, kUploadedFileUrl);
  ASSERT_EQ(uploaded_file_name_, kTargetFileName);
  VerifyBothTokensSetByFetcher();
}

// Case 2b->3: TryUploaderTask() with existing access token but fails with
// authentication error so FetchAccessToken().
TEST_F(RenameHandlerOAuth2Test, StartWithAccessTokenButUploaderOAuth2Error) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenByFetcher, RTokenForFetcher);
  // 2b:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::NotifyOAuth2ErrorForTesting));
  // 3: Authentication error should lead to clearing access token stored, and
  // FetchAccessToken(). Just terminate here though because Case 3 is already
  // covered.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskFailure));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());
  // Verify that access token stored is cleared.
  std::string atoken, rtoken;
  ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken));
  ASSERT_TRUE(atoken.empty());
  ASSERT_EQ(rtoken, RTokenForFetcher);
}

// Case 2b but failed to clear token: Terminate upload flow with internal error.
TEST_F(RenameHandlerOAuth2Test, UploaderOAuth2ErrorThenClearTokenFailure) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenByFetcher, RTokenForFetcher);
  // 2b:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke([this](scoped_refptr<network::SharedURLLoaderFactory>,
                              const std::string&) {
        item_.ClearAllUserData();  // To make GetPrefs() return nullptr.
        uploader()->ExpectNoTryTask(download::DOWNLOAD_INTERRUPT_REASON_CRASH);
        uploader()->NotifyOAuth2ErrorForTesting();
      }));
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());  // Notified failure to terminate.
}

class RenameHandlerOpenDownloadTest : public BrowserWithTestWindowTest,
                                      public RenameHandlerTestBase {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    RenameHandlerTestBase::SetUp(profile());
    EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
    EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
    EXPECT_CALL(*uploader(), TryTask(_, _)).Times(0);
  }

  void TearDown() override {
    RenameHandlerTestBase::TearDown();
    BrowserWithTestWindowTest::TearDown();
  }

  const GURL GetVisibleURL() {
    TabStripModel* tab_strip = browser()->tab_strip_model();
    CHECK(tab_strip);
    content::WebContents* active_contents = tab_strip->GetActiveWebContents();
    CHECK(active_contents);
    return active_contents->GetVisibleURL();
  }
};

TEST_F(RenameHandlerOpenDownloadTest, OpenDownloadItem) {
  EXPECT_CALL(*uploader(), GetUploadedFileUrl())
      .WillOnce(Return(GURL(kUploadedFileUrl)));
  EXPECT_CALL(*uploader(), GetDestinationFolderUrl()).Times(0);

  handler()->OpenDownload();
  // Verify that the active tab has the correct uploaded file URL.
  EXPECT_EQ(GetVisibleURL(), kUploadedFileUrl);
}

TEST_F(RenameHandlerOpenDownloadTest, ShowDownloadInContext) {
  EXPECT_CALL(*uploader(), GetDestinationFolderUrl())
      .WillOnce(Return(GURL(kDestinationFolderUrl)));
  EXPECT_CALL(*uploader(), GetUploadedFileUrl()).Times(0);

  handler()->ShowDownloadInContext();
  // Verify that the active tab has the correct destination folder URL.
  EXPECT_EQ(GetVisibleURL(), kDestinationFolderUrl);
}

}  // namespace enterprise_connectors
