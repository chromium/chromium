// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"

#include <tuple>

#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"
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

class RenameHandlerTestBase : public testing::Test {
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

class FileSystemRenameHandlerCreateTest
    : public RenameHandlerTestBase,
      public testing::WithParamInterface<bool> {
 public:
  FileSystemRenameHandlerCreateTest() {
    Init(enable_feature_flag(), kWildcardSendDownloadToCloudPref);
  }

  bool enable_feature_flag() const { return GetParam(); }
};

TEST_P(FileSystemRenameHandlerCreateTest, Test) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://renameme.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = FileSystemRenameHandler::CreateIfNeeded(&item);
  ASSERT_EQ(enable_feature_flag(), handler.get() != nullptr);
}

INSTANTIATE_TEST_CASE_P(, FileSystemRenameHandlerCreateTest, testing::Bool());

const char ATokenBySignIn[] = "ATokenBySignIn";
const char RTokenBySignIn[] = "RTokenBySignIn";
const char ATokenByFetcher[] = "ATokenByFetcher";
const char RTokenForFetcher[] = "RTokenForFetcher";
class FileSystemRenameHandlerForTest : public FileSystemRenameHandler {
 public:
  using FileSystemRenameHandler::FileSystemRenameHandler;
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

  MOCK_METHOD(void,
              TryControllerTask,
              (content::BrowserContext * context,
               const std::string& access_token),
              (override));

  void ReturnFlowSuccees() {
    GetControllerForTesting()->NotifyResultForTesting(true);
  }

  void ReturnFlowFailure() {
    GetControllerForTesting()->NotifyResultForTesting(false);
  }

  void ReturnFlowOAuth2Error() {
    GetControllerForTesting()->NotifyAuthenFailureForTesting();
  }
};

class FileSystemRenameHandlerTest : public testing::Test {
 public:
  FileSystemRenameHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures({kFileSystemConnectorEnabled}, {});

    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    // Make sure that from the connectors manager point of view the file system
    // connector should be enabled.  So that the only thing that controls
    // whether the rename handler is used or not is the feature flag.
    profile_->GetPrefs()->Set(
        ConnectorPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
        *base::JSONReader::Read(kWildcardSendDownloadToCloudPref));
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);

    item_.SetURL(GURL("https://any.com"));
    content::DownloadItemUtils::AttachInfo(&item_, profile_,
                                           web_contents_.get());

    item_.SetTargetFilePath(base::FilePath::FromUTF8Unsafe("somefile.png"));

    ConnectorsService* service = ConnectorsServiceFactory::GetForBrowserContext(
        content::DownloadItemUtils::GetBrowserContext(&item_));
    auto settings = service->GetFileSystemSettings(
        item_.GetURL(), FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
    handler_ = std::make_unique<FileSystemRenameHandlerForTest>(
        &item_, std::move(settings.value()));
  }

  void SetUp() override {
    testing::Test::SetUp();
    OSCryptMocker::SetUp();
  }

  void TearDown() override {
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

  FileSystemRenameHandlerForTest* handler() { return handler_.get(); }

 private:
  std::unique_ptr<FileSystemRenameHandlerForTest> handler_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::FakeDownloadItem item_;
};

// Test cases are written according to The OAuth2 "Dance" in rename_handler.cc;
// all should be finished via 2a unless aborted via 1b.

////////////////////////////////////////////////////////////////////////////////
// Test cases for (1): Start with no valid token.
////////////////////////////////////////////////////////////////////////////////

// Case 1a->2a: Both tokens will be set; callback returned with success.
TEST_F(FileSystemRenameHandlerTest, SignInSuccessThenControllerSucess) {
  ::testing::InSequence seq;
  // 1a: PromptUserSignInForAuthorization() succeeds.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnSignInSuccess));
  // ->2a: TryControllerTask() should be called after and succeed.
  EXPECT_CALL(*handler(), TryControllerTask(_, _))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnFlowSuccees));
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
TEST_F(FileSystemRenameHandlerTest, SignInCancellationSoAbort) {
  ::testing::InSequence seq;
  // 1b: PromptUserSignInForAuthorization() fails with Cancellation so abort.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(
          Invoke(handler(),
                 &FileSystemRenameHandlerForTest::ReturnSignInCancellation));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  EXPECT_CALL(*handler(), TryControllerTask(_, _)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  VerifyBothTokensClear();
}

// Case 1c->1a: Retry sign in, but terminate there without going through the
// controller since 1a->2 is already covered in another test case.
TEST_F(FileSystemRenameHandlerTest, SignInFailureSoRetry) {
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
              handler()->ReturnFlowSuccees();
              // Terminate here since 1a->2 is already covered.
            } else {
              FAIL() << "Should've already successfully obtained tokens above";
            }
          }));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  EXPECT_CALL(*handler(), TryControllerTask(_, _)).Times(0);

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
// TryControllerTask();
TEST_F(FileSystemRenameHandlerTest, FetchAccessTokenSuccess) {
  ::testing::InSequence seq;
  // 3a: Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), "box", std::string(), RTokenForFetcher);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnFetchSuccess));
  // ->2a.
  EXPECT_CALL(*handler(), TryControllerTask(_, _))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnFlowSuccees));
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
TEST_F(FileSystemRenameHandlerTest, FetchAccessTokenFailureSoPromptForSignIn) {
  ::testing::InSequence seq;
  // 3a: Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), "box", std::string(), RTokenForFetcher);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnFetchFailure));
  // ->1: Prompt user to sign in, but terminate because Case 1 is already
  // covered.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnFlowSuccees));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), TryControllerTask(_, _)).Times(0);

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

// Case 2a(failure): TryControllerTask() with existing access token and fails,
// but both tokens stay.
TEST_F(FileSystemRenameHandlerTest, ControllerFailure) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryControllerTask().
  SetFileSystemOAuth2Tokens(prefs(), "box", ATokenByFetcher, RTokenForFetcher);
  // 2a:
  EXPECT_CALL(*handler(), TryControllerTask(_, _))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnFlowFailure));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  int download_callback;
  download::DownloadInterruptReason download_interrupt_reason;
  RunHandler(&download_callback, &download_interrupt_reason);

  ASSERT_EQ(download_callback, 1);
  ASSERT_EQ(download_interrupt_reason,
            download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  // Verify that controller failure did not affect stored credentials.
  VerifyBothTokensSetByFetcher();
}

// Case 2a(success): TryControllerTask() with existing access token and
// succeeds.
TEST_F(FileSystemRenameHandlerTest, StartWithAccessTokenThenControllerSuccess) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryControllerTask().
  SetFileSystemOAuth2Tokens(prefs(), "box", ATokenByFetcher, RTokenForFetcher);
  // 2a:
  EXPECT_CALL(*handler(), TryControllerTask(_, _))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnFlowSuccees));
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

// Case 2b->3: TryControllerTask() with existing access token but fails with
// authentication error so FetchAccessToken().
TEST_F(FileSystemRenameHandlerTest,
       StartWithAccessTokenButControllerAuthenticationError) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryControllerTask().
  SetFileSystemOAuth2Tokens(prefs(), "box", ATokenByFetcher, RTokenForFetcher);
  // 2b:
  EXPECT_CALL(*handler(), TryControllerTask(_, _))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnFlowOAuth2Error));
  // 3: Authentication error should lead to clearing access token stored, and
  // FetchAccessToken(). Just terminate here though because Case 3 is already
  // covered.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(handler(),
                       &FileSystemRenameHandlerForTest::ReturnFlowSuccees));
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

class RenameHandlerUrlTest : public RenameHandlerTestBase {
 public:
  RenameHandlerUrlTest() { Init(true, kRenameSendDownloadToCloudPref); }
};

TEST_F(RenameHandlerUrlTest, NoUrlMatchesPattern) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://one.com/file.txt"));
  item.SetTabUrl(GURL("https://two.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = FileSystemRenameHandler::CreateIfNeeded(&item);
  ASSERT_EQ(nullptr, handler.get());
}

TEST_F(RenameHandlerUrlTest, FileUrlMatchesPattern) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://renameme.com/file.txt"));
  item.SetTabUrl(GURL("https://two.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = FileSystemRenameHandler::CreateIfNeeded(&item);
  ASSERT_NE(nullptr, handler.get());
}

TEST_F(RenameHandlerUrlTest, TabUrlMatchesPattern) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://one.com/file.txt"));
  item.SetTabUrl(GURL("https://renameme.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = FileSystemRenameHandler::CreateIfNeeded(&item);
  ASSERT_NE(nullptr, handler.get());
}

TEST_F(RenameHandlerUrlTest, DisallowByMimeType) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://renameme.com/file.json"));
  item.SetTabUrl(GURL("https://renameme.com"));
  item.SetMimeType("application/json");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = FileSystemRenameHandler::CreateIfNeeded(&item);
  ASSERT_EQ(nullptr, handler.get());
}

class RenameHandlerWildcardTest : public RenameHandlerTestBase {
 public:
  RenameHandlerWildcardTest() { Init(true, kWildcardSendDownloadToCloudPref); }
};

TEST_F(RenameHandlerWildcardTest, AllowedByWildcard) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://one.com/file.txt"));
  item.SetTabUrl(GURL("https://two.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = FileSystemRenameHandler::CreateIfNeeded(&item);
  ASSERT_NE(nullptr, handler.get());
}

}  // namespace enterprise_connectors
