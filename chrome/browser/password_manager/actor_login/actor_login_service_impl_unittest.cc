// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_service_impl.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_delegate.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_quality_logger.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor_login {

namespace {

using base::test::RunOnceCallback;
using testing::_;
using testing::Eq;
using testing::Return;

Credential CreateTestCredential() {
  Credential credential;
  credential.username = u"testusername";
  credential.source_site_or_app = u"example.com";
  credential.type = CredentialType::kPassword;
  return credential;
}

struct AttemptLoginTestCase {
  LoginStatusResult result;
  AttemptLoginResult metric;
  std::string test_case_name;
};

}  // namespace

class ActorLoginServiceImplTest : public testing::Test {
 public:
  ActorLoginServiceImplTest() {
    service_ = std::make_unique<ActorLoginServiceImpl>();
    service_->SetActorLoginDelegateFactoryForTesting(base::BindRepeating(
        [](MockActorLoginDelegate* delegate,
           content::WebContents*) -> ActorLoginDelegate* { return delegate; },
        base::Unretained(&mock_delegate_)));
  }

  base::WeakPtr<MockActorLoginQualityLogger> mqls_logger() {
    return mock_mqls_logger_.AsWeakPtr();
  }

 protected:
  // Needed by `TestingProfile`
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory test_web_contents_factory_;
  MockActorLoginDelegate mock_delegate_;
  std::unique_ptr<ActorLoginServiceImpl> service_;
  MockActorLoginQualityLogger mock_mqls_logger_;
};

TEST_F(ActorLoginServiceImplTest, GetCredentialsInvalidTabInterface) {
  base::HistogramTester histogram_tester;
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillOnce(Return(nullptr));

  base::test::TestFuture<CredentialsOrError> future;
  EXPECT_CALL(mock_delegate_, GetCredentials).Times(0);
  service_->GetCredentials(&mock_tab, mqls_logger(), future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kInvalidTabInterface);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.GetCredentials.Result",
      GetCredentialsResult::kErrorInvalidTabInterface, 1);
}

TEST_F(ActorLoginServiceImplTest, GetCredentialsDelegatesToActorLoginDelegate) {
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));

  EXPECT_CALL(mock_delegate_, GetCredentials);
  service_->GetCredentials(&mock_tab, mqls_logger(), base::DoNothing());
}

TEST_F(ActorLoginServiceImplTest, GetCredentials_Success) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));

  std::vector<Credential> credentials;
  credentials.push_back(CreateTestCredential());

  base::test::TestFuture<CredentialsOrError> future;
  EXPECT_CALL(mock_delegate_, GetCredentials)
      .WillOnce(RunOnceCallback<1>(credentials));
  service_->GetCredentials(&mock_tab, mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  ASSERT_EQ(future.Get().value().size(), 1u);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.GetCredentials.Result",
      GetCredentialsResult::kSuccess, 1);
}

TEST_F(ActorLoginServiceImplTest, GetCredentials_ServiceBusy) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));

  base::test::TestFuture<CredentialsOrError> future;
  EXPECT_CALL(mock_delegate_, GetCredentials)
      .WillOnce(RunOnceCallback<1>(
          base::unexpected(ActorLoginError::kFillingNotAllowed)));
  service_->GetCredentials(&mock_tab, mqls_logger(), future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kFillingNotAllowed);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.GetCredentials.Result",
      GetCredentialsResult::kErrorFillingNotAllowed, 1);
}

TEST_F(ActorLoginServiceImplTest, GetCredentials_FillingNotAllowed) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));

  base::test::TestFuture<CredentialsOrError> future;
  EXPECT_CALL(mock_delegate_, GetCredentials)
      .WillOnce(
          RunOnceCallback<1>(base::unexpected(ActorLoginError::kServiceBusy)));
  service_->GetCredentials(&mock_tab, mqls_logger(), future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kServiceBusy);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.GetCredentials.Result",
      GetCredentialsResult::kErrorServiceBusy, 1);
}

TEST_F(ActorLoginServiceImplTest, AttemptLoginInvalidTabInterface) {
  base::HistogramTester histogram_tester;
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillOnce(Return(nullptr));

  Credential credential = CreateTestCredential();
  base::test::TestFuture<LoginStatusResultOrError> future;
  EXPECT_CALL(mock_delegate_, AttemptLogin).Times(0);
  service_->AttemptLogin(&mock_tab, credential, false, mqls_logger(),
                         future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kInvalidTabInterface);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.AttemptLogin.Result",
      AttemptLoginResult::kErrorInvalidTabInterface, 1);
}

TEST_F(ActorLoginServiceImplTest, AttemptLoginDelegatesToActorLoginDelegate) {
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));
  Credential credential = CreateTestCredential();

  EXPECT_CALL(mock_delegate_, AttemptLogin(Eq(credential), _, _, _));
  service_->AttemptLogin(&mock_tab, credential, false, mqls_logger(),
                         base::DoNothing());
}

TEST_F(ActorLoginServiceImplTest, AttemptLogin_ServiceBusy) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));
  Credential credential = CreateTestCredential();

  base::test::TestFuture<LoginStatusResultOrError> future;
  EXPECT_CALL(mock_delegate_, AttemptLogin(Eq(credential), _, _, _))
      .WillOnce(
          RunOnceCallback<3>(base::unexpected(ActorLoginError::kServiceBusy)));
  service_->AttemptLogin(&mock_tab, credential, false, mqls_logger(),
                         future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kServiceBusy);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.AttemptLogin.Result",
      AttemptLoginResult::kErrorServiceBusy, 1);
}

TEST_F(ActorLoginServiceImplTest, AttemptLogin_FillingNotAllowed) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));
  Credential credential = CreateTestCredential();

  base::test::TestFuture<LoginStatusResultOrError> future;
  EXPECT_CALL(mock_delegate_, AttemptLogin(Eq(credential), _, _, _))
      .WillOnce(RunOnceCallback<3>(
          base::unexpected(ActorLoginError::kFillingNotAllowed)));
  service_->AttemptLogin(&mock_tab, credential, false, mqls_logger(),
                         future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kFillingNotAllowed);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.AttemptLogin.Result",
      AttemptLoginResult::kErrorFillingNotAllowed, 1);
}

class ActorLoginServiceImplAttemptLoginTest
    : public ActorLoginServiceImplTest,
      public testing::WithParamInterface<AttemptLoginTestCase> {};

TEST_P(ActorLoginServiceImplAttemptLoginTest, AttemptLoginResults) {
  const AttemptLoginTestCase& test_case = GetParam();
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));
  Credential credential = CreateTestCredential();

  base::test::TestFuture<LoginStatusResultOrError> future;
  EXPECT_CALL(mock_delegate_, AttemptLogin(Eq(credential), _, _, _))
      .WillOnce(RunOnceCallback<3>(test_case.result));
  service_->AttemptLogin(&mock_tab, credential, false, mqls_logger(),
                         future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), test_case.result);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.AttemptLogin.Result", test_case.metric, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ActorLoginServiceImplAttemptLoginTest,
    testing::Values(
        AttemptLoginTestCase{LoginStatusResult::kErrorNoSigninForm,
                             AttemptLoginResult::kErrorNoSigninForm,
                             "ErrorNoSigninForm"},
        AttemptLoginTestCase{LoginStatusResult::kErrorInvalidCredential,
                             AttemptLoginResult::kErrorInvalidCredential,
                             "ErrorInvalidCredential"},
        AttemptLoginTestCase{
            LoginStatusResult::kSuccessUsernameAndPasswordFilled,
            AttemptLoginResult::kSuccessUsernameAndPasswordFilled,
            "SuccessUsernameAndPasswordFilled"},
        AttemptLoginTestCase{LoginStatusResult::kSuccessUsernameFilled,
                             AttemptLoginResult::kSuccessUsernameFilled,
                             "SuccessUsernameFilled"},
        AttemptLoginTestCase{LoginStatusResult::kSuccessPasswordFilled,
                             AttemptLoginResult::kSuccessPasswordFilled,
                             "SuccessPasswordFilled"},
        AttemptLoginTestCase{LoginStatusResult::kErrorNoFillableFields,
                             AttemptLoginResult::kErrorNoFillableFields,
                             "ErrorNoFillableFields"},
        AttemptLoginTestCase{LoginStatusResult::kErrorDeviceReauthRequired,
                             AttemptLoginResult::kErrorDeviceReauthRequired,
                             "ErrorDeviceReauthRequired"},
        AttemptLoginTestCase{LoginStatusResult::kErrorDeviceReauthFailed,
                             AttemptLoginResult::kErrorDeviceReauthFailed,
                             "ErrorDeviceReauthFailed"}),

    [](const testing::TestParamInfo<AttemptLoginTestCase>& info) {
      return info.param.test_case_name;
    });

}  // namespace actor_login
