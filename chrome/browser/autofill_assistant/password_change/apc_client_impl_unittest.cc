// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_move_support.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"
#include "chrome/browser/autofill_assistant/password_change/mock_apc_onboarding_coordinator.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr char kUrl1[] = "https://www.example.com";
constexpr char kUsername1[] = "Lori";
}  // namespace

class TestApcClientImpl : public ApcClientImpl {
 public:
  static TestApcClientImpl* CreateForWebContents(
      content::WebContents* web_contents);

  explicit TestApcClientImpl(content::WebContents* web_contents)
      : ApcClientImpl(web_contents) {}

  std::unique_ptr<ApcOnboardingCoordinator> CreateOnboardingCoordinator()
      override {
    return std::move(coordinator_);
  }

  // Allows setting an onboarding coordinator that is returned by the factory
  // function. Must be called at least once before every exepcted call to
  // `CreateOnboardingCoordinator()`.
  void InjectOnboardingCoordinatorForTesting(
      std::unique_ptr<ApcOnboardingCoordinator> coordinator) {
    coordinator_ = std::move(coordinator);
  }

 private:
  std::unique_ptr<ApcOnboardingCoordinator> coordinator_;
};

// static
TestApcClientImpl* TestApcClientImpl::CreateForWebContents(
    content::WebContents* web_contents) {
  const void* key = WebContentsUserData<ApcClientImpl>::UserDataKey();
  web_contents->SetUserData(key,
                            std::make_unique<TestApcClientImpl>(web_contents));
  return static_cast<TestApcClientImpl*>(web_contents->GetUserData(key));
}

class ApcClientImplTest : public testing::Test {
 public:
  ApcClientImplTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&profile_)) {
    // Make sure that a `TestApcClientImpl` is registered for that
    // `WebContents`.
    test_apc_client_ = TestApcClientImpl::CreateForWebContents(web_contents());
  }

  TestApcClientImpl* apc_client() { return test_apc_client_; }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  // Supporting members to create the testing environment.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;

  // The object that is tested.
  raw_ptr<TestApcClientImpl> test_apc_client_;
};

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_WithFailedOnboarding) {
  raw_ptr<ApcClient> client =
      ApcClient::GetOrCreateForWebContents(web_contents());

  // There is one client per WebContents.
  EXPECT_EQ(client, apc_client());

  // The `ApcClient` is paused.
  EXPECT_FALSE(client->IsRunning());

  // Prepare the coordinator.
  raw_ptr<MockApcOnboardingCoordinator> coordinator =
      new MockApcOnboardingCoordinator();
  apc_client()->InjectOnboardingCoordinatorForTesting(
      base::WrapUnique<MockApcOnboardingCoordinator>(coordinator));

  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator, PerformOnboarding)
      .WillOnce(MoveArg<0>(&coordinator_callback));

  EXPECT_TRUE(client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true));
  EXPECT_TRUE(client->IsRunning());

  // We cannot start a second flow.
  EXPECT_FALSE(client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true));

  // Fail onboarding.
  std::move(coordinator_callback).Run(false);
  EXPECT_FALSE(client->IsRunning());
}
