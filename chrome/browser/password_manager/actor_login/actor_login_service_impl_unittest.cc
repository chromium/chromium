// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_service_impl.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_delegate.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor_login {

namespace {

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

 protected:
  // Needed by `TestingProfile`
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory test_web_contents_factory_;
  MockActorLoginDelegate mock_delegate_;
  std::unique_ptr<ActorLoginServiceImpl> service_;
};

TEST_F(ActorLoginServiceImplTest, GetCredentialsInvalidTabInterface) {
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillOnce(Return(nullptr));

  base::test::TestFuture<CredentialsOrError> future;
  EXPECT_CALL(mock_delegate_, GetCredentials).Times(0);
  service_->GetCredentials(&mock_tab, future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kInvalidTabInterface);
}

TEST_F(ActorLoginServiceImplTest, GetCredentialsDelegatesToActorLoginDelegate) {
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));

  EXPECT_CALL(mock_delegate_, GetCredentials);
  service_->GetCredentials(&mock_tab, base::DoNothing());
}

TEST_F(ActorLoginServiceImplTest, AttemptLoginInvalidTabInterface) {
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillOnce(Return(nullptr));

  Credential credential = CreateTestCredential();
  base::test::TestFuture<LoginStatusResultOrError> future;
  EXPECT_CALL(mock_delegate_, AttemptLogin).Times(0);
  service_->AttemptLogin(&mock_tab, credential, future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kInvalidTabInterface);
}

TEST_F(ActorLoginServiceImplTest, AttemptLoginDelegatesToActorLoginDelegate) {
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));
  Credential credential = CreateTestCredential();

  EXPECT_CALL(mock_delegate_, AttemptLogin(Eq(credential), _));
  service_->AttemptLogin(&mock_tab, credential, base::DoNothing());
}

}  // namespace actor_login
