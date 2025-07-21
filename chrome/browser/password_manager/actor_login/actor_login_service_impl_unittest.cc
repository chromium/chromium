// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_service_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor_login {

using testing::Return;

class ActorLoginServiceImplTest : public testing::Test {
 public:
  ActorLoginServiceImplTest() {
    service_ = std::make_unique<ActorLoginServiceImpl>();
  }

 protected:
  // Needed by `TestingProfile`
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory test_web_contents_factory_;
  std::unique_ptr<ActorLoginServiceImpl> service_;
};

TEST_F(ActorLoginServiceImplTest, GetCredentialsInvalidTabInterface) {
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillOnce(Return(nullptr));

  base::test::TestFuture<CredentialsOrError> future;
  service_->GetCredentials(&mock_tab, future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kInvalidTabInterface);
}

TEST_F(ActorLoginServiceImplTest, GetCredentialsDelegatesToActorLoginDelegate) {
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));

  base::test::TestFuture<CredentialsOrError> future;
  service_->GetCredentials(&mock_tab, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());  // Delegate returns empty vector
}

TEST_F(ActorLoginServiceImplTest, AttemptLoginInvalidTabInterface) {
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillOnce(Return(nullptr));

  Credential credential;
  base::test::TestFuture<LoginStatusResultOrError> future;
  service_->AttemptLogin(&mock_tab, credential, future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kInvalidTabInterface);
}

TEST_F(ActorLoginServiceImplTest, AttemptLoginDelegatesToActorLoginDelegate) {
  content::WebContents* web_contents =
      test_web_contents_factory_.CreateWebContents(&profile_);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents()).WillRepeatedly(Return(web_contents));

  Credential credential;
  base::test::TestFuture<LoginStatusResultOrError> future;
  service_->AttemptLogin(&mock_tab, credential, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_FALSE(future.Get().value().value());  // Delegate returns default false
}

}  // namespace actor_login
