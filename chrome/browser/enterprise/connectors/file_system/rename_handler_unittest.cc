// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"

#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace enterprise_connectors {

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
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://renameme.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = RenameHandler::CreateIfNeeded(&item);
  ASSERT_EQ(enable_feature_flag(), handler.get() != nullptr);
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

  MOCK_METHOD(void,
              PromptUserSignInForAuthorization,
              (content::WebContents * contents),
              (override));

  void ReturnSignInSuccess() {
    OnAuthorization(GoogleServiceAuthError::AuthErrorNone(), ATokenBySignIn,
                    RTokenBySignIn);
  }

  void ReturnSignInFailure() {
    OnAuthorization(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                        GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                            CREDENTIALS_REJECTED_BY_SERVER),
                    std::string(), std::string());
  }

  void ReturnSignInCancellation() {
    OnAuthorization(
        GoogleServiceAuthError{GoogleServiceAuthError::State::REQUEST_CANCELED},
        std::string(), std::string());
  }

  MOCK_METHOD(void,
              FetchAccessToken,
              (content::BrowserContext * context,
               const std::string& refresh_token),
              (override));

  void ReturnFetchSuccess() {
    OnAccessTokenFetched(GoogleServiceAuthError::AuthErrorNone(),
                         ATokenByFetcher, RTokenForFetcher);
  }

  void ReturnFetchFailure() {
    OnAccessTokenFetched(
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER),
        std::string(), std::string());
  }
};

const char kUploadedFileUrl[] = "https://example.com/file/314159";
const char kDestinationFolderUrl[] = "https://example.com/folder/1337";

class MockUploader : public BoxUploader {
 public:
  explicit MockUploader(download::DownloadItem* download_item)
      : BoxUploader(download_item) {}
  GURL GetUploadedFileUrl() const override { return GURL(kUploadedFileUrl); }
  GURL GetDestinationFolderUrl() const override {
    return GURL(kDestinationFolderUrl);
  }

  MOCK_METHOD(
      void,
      TryTask,
      (scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
       const std::string& access_token),
      (override));

  MOCK_METHOD(std::unique_ptr<OAuth2ApiCallFlow>,
              MakeFileUploadApiCall,
              (),
              (override));

  void NotifySuccess() { NotifyResultForTesting(true); }

  void NotifyFailure() { NotifyResultForTesting(false); }
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

    item_.SetURL(GURL("https://any.com"));
    content::DownloadItemUtils::AttachInfo(&item_, profile,
                                           web_contents_.get());

    item_.SetTargetFilePath(base::FilePath::FromUTF8Unsafe("somefile.png"));

    ConnectorsService* service = ConnectorsServiceFactory::GetForBrowserContext(
        content::DownloadItemUtils::GetBrowserContext(&item_));
    auto settings = service->GetFileSystemSettings(
        item_.GetURL(), FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
    settings->service_provider = "box";
    handler_ = std::make_unique<RenameHandlerForTest>(
        &item_, std::move(settings.value()));

    auto uploader = std::make_unique<MockUploader>(&item_);
    uploader_ = uploader.get();
    handler_->SetUploaderForTesting(std::move(uploader));

    EXPECT_CALL(*uploader_, MakeFileUploadApiCall()).Times(0);
  }

  void TearDown() {
    handler_.reset();
    web_contents_.reset();
  }

  content::FakeDownloadItem item_;

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
    testing::Test::SetUp();
    OSCryptMocker::SetUp();
    RenameHandlerTestBase::SetUp(profile_);
  }

  void TearDown() override {
    RenameHandlerTestBase::TearDown();
    OSCryptMocker::TearDown();
    testing::Test::TearDown();
  }

  void RunHandler(
      int* download_callback,
      download::DownloadInterruptReason* download_interrupt_reason) {
    int local_download_callback = 0;
    download::DownloadInterruptReason local_download_interrupt_reason;
    base::RunLoop run_loop;
    download::DownloadItemRenameHandler* download_handler_ = handler();
    download_handler_->Start(base::BindLambdaForTesting(
        [&local_download_callback, &run_loop, &local_download_interrupt_reason](
            download::DownloadInterruptReason reason,
            const base::FilePath& path) {
          ++local_download_callback;
          local_download_interrupt_reason = reason;
          run_loop.Quit();
        }));
    run_loop.Run();
    if (download_callback)
      *download_callback = local_download_callback;
    if (download_interrupt_reason)
      *download_interrupt_reason = local_download_interrupt_reason;
  }

  void VerifyBothTokensClear() {
    std::string atoken, rtoken;
    ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), "box", &atoken, &rtoken))
        << "Access Token: " << atoken << "\nRefresh Token: " << rtoken;
    ASSERT_TRUE(atoken.empty());
    ASSERT_TRUE(rtoken.empty());
  }

  void VerifyBothTokensSetBySignIn() {
    std::string atoken, rtoken;
    ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), "box", &atoken, &rtoken))
        << "Access Token: " << atoken << "\nRefresh Token: " << rtoken;
    ASSERT_EQ(atoken, ATokenBySignIn);
    ASSERT_EQ(rtoken, RTokenBySignIn);
  }

  void VerifyBothTokensSetByFetcher() {
    std::string atoken, rtoken;
    ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), "box", &atoken, &rtoken))
        << "Access Token: " << atoken << "\nRefresh Token: " << rtoken;
    ASSERT_EQ(atoken, ATokenByFetcher);
    ASSERT_EQ(rtoken, RTokenForFetcher);
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

 private:

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
};

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
      .WillOnce(Invoke(uploader(), &MockUploader::NotifySuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_NONE);
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
  EXPECT_CALL(*uploader(), TryTask(_, _)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  VerifyBothTokensClear();
}

// Case 1c->1a: Retry sign in, but terminate there without going through the
// uploader since 1a->2 is already covered in another test case.
TEST_F(RenameHandlerOAuth2Test, SignInFailureSoRetry) {
  ::testing::InSequence seq;
  // 1c->1a: PromptUserSignInForAuthorization() fails with other reasons than
  // Cancellation so should be called again.
  int authen_callback = 0;
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillRepeatedly(
          Invoke([this, &authen_callback](content::WebContents* contents) {
            ++authen_callback;
            if (authen_callback == 1) {
              handler()->ReturnSignInFailure();
            } else if (authen_callback == 2) {
              VerifyBothTokensClear();
              uploader()->NotifySuccess();
              // Terminate here since 1a->2 is already covered.
            } else {
              FAIL() << "Should've already successfully obtained tokens above";
            }
          }));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  EXPECT_CALL(*uploader(), TryTask(_, _)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(authen_callback, 2);
  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_NONE);
  VerifyBothTokensClear();
}

////////////////////////////////////////////////////////////////////////////////
// Test cases for (3): Start with only refresh token.
////////////////////////////////////////////////////////////////////////////////

// Case 3a->2a: Fetch access token with refresh token succeeds, so should
// TryUploaderTask();
TEST_F(RenameHandlerOAuth2Test, FetchAccessTokenSuccess) {
  ::testing::InSequence seq;
  // 3a: Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), "box", std::string(), RTokenForFetcher);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(handler(), &RenameHandlerForTest::ReturnFetchSuccess));
  // ->2a.
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::NotifySuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_NONE);
  VerifyBothTokensSetByFetcher();
}

// Case 3b->1: Fetch access token with refresh token fails, so should clear
// tokens and PromptUserSignInForAuthorization().
TEST_F(RenameHandlerOAuth2Test, FetchAccessTokenFailureSoPromptForSignIn) {
  ::testing::InSequence seq;
  // 3a: Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), "box", std::string(), RTokenForFetcher);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(handler(), &RenameHandlerForTest::ReturnFetchFailure));
  // ->1: Prompt user to sign in, but terminate because Case 1 is already
  // covered.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(Invoke(uploader(), &MockUploader::NotifySuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*uploader(), TryTask(_, _)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_NONE);
  VerifyBothTokensClear();
}

////////////////////////////////////////////////////////////////////////////////
// Test cases for (2): Start with both tokens.
////////////////////////////////////////////////////////////////////////////////

// Case 2a(failure): TryUploaderTask() with existing access token and fails,
// but both tokens stay.
TEST_F(RenameHandlerOAuth2Test, UploaderFailure) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), "box", ATokenByFetcher, RTokenForFetcher);
  // 2a:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::NotifyFailure));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  // Verify that uploader failure did not affect stored credentials.
  VerifyBothTokensSetByFetcher();
}

// Case 2a(success): TryUploaderTask() with existing access token and
// succeeds.
TEST_F(RenameHandlerOAuth2Test, StartWithAccessTokenThenUploaderSuccess) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), "box", ATokenByFetcher, RTokenForFetcher);
  // 2a:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::NotifySuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_NONE);
  VerifyBothTokensSetByFetcher();
}

// Case 2b->3: TryUploaderTask() with existing access token but fails with
// authentication error so FetchAccessToken().
TEST_F(RenameHandlerOAuth2Test, StartWithAccessTokenButUploaderOAuth2Error) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), "box", ATokenByFetcher, RTokenForFetcher);
  // 2b:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::NotifyOAuth2ErrorForTesting));
  // 3: Authentication error should lead to clearing access token stored, and
  // FetchAccessToken(). Just terminate here though because Case 3 is already
  // covered.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::NotifySuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_NONE);
  // Verify that access token stored is cleared.
  std::string atoken, rtoken;
  ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), "box", &atoken, &rtoken));
  ASSERT_TRUE(atoken.empty());
  ASSERT_EQ(rtoken, RTokenForFetcher);
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
  handler()->OpenDownload();
  // Verify that the active tab has the correct uploaded file URL.
  EXPECT_EQ(GetVisibleURL(), kUploadedFileUrl);
}

TEST_F(RenameHandlerOpenDownloadTest, ShowDownloadInContext) {
  handler()->ShowDownloadInContext();
  // Verify that the active tab has the correct destination folder URL.
  EXPECT_EQ(GetVisibleURL(), kDestinationFolderUrl);
}

}  // namespace enterprise_connectors
