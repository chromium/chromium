// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_downloader.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestEmail[] = "test@example.com";
const char kTestHostedDomain[] = "google.com";
const char kTestFullName[] = "full_name";
const char kTestGivenName[] = "given_name";
const char kTestLocale[] = "locale";
const char kTestValidPictureURL[] = "http://www.google.com/";
const char kTestInvalidPictureURL[] = "invalid_picture_url";

} // namespace

class ProfileDownloaderTest
    : public testing::Test,
      public ProfileDownloaderDelegate,
      public signin::IdentityManager::DiagnosticsObserver {
 protected:
  ProfileDownloaderTest() : profile_downloader_(this) {
    identity_test_env_.identity_manager()->AddDiagnosticsObserver(this);
  }
  ~ProfileDownloaderTest() override {
    identity_test_env_.identity_manager()->RemoveDiagnosticsObserver(this);
  }

  bool NeedsProfilePicture() const override { return true; }
  int GetDesiredImageSideLength() const override { return 128; }
  std::string GetCachedPictureURL() const override { return std::string(); }
  signin::IdentityManager* GetIdentityManager() override {
    return identity_test_env_.identity_manager();
  }
  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override {
    return &test_url_loader_factory_;
  }
  bool IsPreSignin() const override { return false; }
  void OnProfileDownloadSuccess(ProfileDownloader* downloader) override {

  }
  void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) override {}

  void SimulateUserInfoSuccess(const std::string& picture_url,
                               const AccountInfo& account_info) {
    identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        kTestHostedDomain, kTestFullName, kTestGivenName, kTestLocale,
        picture_url);
  }

  // IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRequested(const CoreAccountId& account_id,
                              const std::string& consumer_id,
                              const identity::ScopeSet& scopes) override {
    // This flow should be invoked only when a test has explicitly set up
    // preconditions so that ProfileDownloader will request access tokens.
    DCHECK(!on_access_token_request_callback_.is_null());

    account_id_for_access_token_request_ = account_id;

    std::move(on_access_token_request_callback_).Run();
  }

  void set_on_access_token_requested_callback(base::OnceClosure callback) {
    on_access_token_request_callback_ = std::move(callback);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  ProfileDownloader profile_downloader_;

  base::OnceClosure on_access_token_request_callback_;
  CoreAccountId account_id_for_access_token_request_;
};

TEST_F(ProfileDownloaderTest, FetchAccessToken) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(kTestEmail);
  identity_test_env_.SetRefreshTokenForAccount(account_info.account_id);

  base::RunLoop run_loop;
  set_on_access_token_requested_callback(run_loop.QuitClosure());
  profile_downloader_.StartForAccount(account_info.account_id);
  run_loop.Run();

  EXPECT_EQ(account_info.account_id, account_id_for_access_token_request_);
}

TEST_F(ProfileDownloaderTest, AccountInfoReady) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(kTestEmail);
  SimulateUserInfoSuccess(kTestValidPictureURL, account_info);

  ASSERT_EQ(ProfileDownloader::PICTURE_FAILED,
            profile_downloader_.GetProfilePictureStatus());
  base::RunLoop run_loop;
  set_on_access_token_requested_callback(run_loop.QuitClosure());
  profile_downloader_.StartForAccount(account_info.account_id);
  run_loop.Run();
  profile_downloader_.StartFetchingImage();
  ASSERT_EQ(kTestValidPictureURL, profile_downloader_.GetProfilePictureURL());
}

TEST_F(ProfileDownloaderTest, AccountInfoNotReady) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(kTestEmail);
  ASSERT_EQ(ProfileDownloader::PICTURE_FAILED,
            profile_downloader_.GetProfilePictureStatus());
  base::RunLoop run_loop;
  set_on_access_token_requested_callback(run_loop.QuitClosure());
  profile_downloader_.StartForAccount(account_info.account_id);
  run_loop.Run();
  profile_downloader_.StartFetchingImage();
  SimulateUserInfoSuccess(kTestValidPictureURL, account_info);
  ASSERT_EQ(kTestValidPictureURL, profile_downloader_.GetProfilePictureURL());
}

// Regression test for http://crbug.com/854907
TEST_F(ProfileDownloaderTest, AccountInfoNoPictureDoesNotCrash) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(kTestEmail);
  SimulateUserInfoSuccess(kNoPictureURLFound, account_info);

  base::RunLoop run_loop;
  set_on_access_token_requested_callback(run_loop.QuitClosure());
  profile_downloader_.StartForAccount(account_info.account_id);
  run_loop.Run();
  profile_downloader_.StartFetchingImage();

  EXPECT_TRUE(profile_downloader_.GetProfilePictureURL().empty());
  ASSERT_EQ(ProfileDownloader::PICTURE_DEFAULT,
            profile_downloader_.GetProfilePictureStatus());
}

// Regression test for http://crbug.com/854907
TEST_F(ProfileDownloaderTest, AccountInfoInvalidPictureURLDoesNotCrash) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(kTestEmail);
  SimulateUserInfoSuccess(kTestInvalidPictureURL, account_info);

  base::RunLoop run_loop;
  set_on_access_token_requested_callback(run_loop.QuitClosure());
  profile_downloader_.StartForAccount(account_info.account_id);
  run_loop.Run();
  profile_downloader_.StartFetchingImage();

  EXPECT_TRUE(profile_downloader_.GetProfilePictureURL().empty());
  ASSERT_EQ(ProfileDownloader::PICTURE_FAILED,
            profile_downloader_.GetProfilePictureStatus());
}
