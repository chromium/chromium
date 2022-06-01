// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"
#include "chrome/browser/autofill_assistant/password_change/mock_apc_onboarding_coordinator.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill_assistant/browser/public/mock_external_script_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr char kUrl1[] = "https://www.example.com";
constexpr char kUsername1[] = "Lori";

constexpr char kPasswordChangeSkipLoginParameter[] =
    "PASSWORD_CHANGE_SKIP_LOGIN";
constexpr char kSourceParameter[] = "SOURCE";
constexpr char kSourcePasswordChangeLeakWarning[] = "10";
constexpr char kSourcePasswordChangeSettings[] = "11";
}  // namespace

using ::testing::DoAll;
using ::testing::StrEq;

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

  std::unique_ptr<autofill_assistant::ExternalScriptController>
  CreateExternalScriptController() override {
    return std::move(external_script_controller_);
  }

  // Allows setting an onboarding coordinator that is returned by the factory
  // function. Must be called at least once before every expected call to
  // `CreateOnboardingCoordinator()`.
  void InjectOnboardingCoordinatorForTesting(
      std::unique_ptr<ApcOnboardingCoordinator> coordinator) {
    coordinator_ = std::move(coordinator);
  }

  // Allows setting an ExternalScriptController. Must be called at least once
  // before every expected call to `CreateExternalScriptController()`.
  void InjectExternalScriptControllerForTesting(
      std::unique_ptr<autofill_assistant::ExternalScriptController>
          external_script_controller) {
    external_script_controller_ = std::move(external_script_controller);
  }

 private:
  std::unique_ptr<ApcOnboardingCoordinator> coordinator_;
  std::unique_ptr<autofill_assistant::ExternalScriptController>
      external_script_controller_;
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
    feature_list_.InitWithFeatures({features::kUnifiedSidePanel}, {});
    // Make sure that a `TestApcClientImpl` is registered for that
    // `WebContents`.
    test_apc_client_ = TestApcClientImpl::CreateForWebContents(web_contents());

    // Prepare the coordinator.
    auto coordinator = std::make_unique<MockApcOnboardingCoordinator>();
    coordinator_ref_ = coordinator.get();
    test_apc_client_->InjectOnboardingCoordinatorForTesting(
        std::move(coordinator));

    // Prepare the ExternalScriptController.
    auto external_script_controller =
        std::make_unique<autofill_assistant::MockExternalScriptController>();
    external_script_controller_ref_ = external_script_controller.get();
    test_apc_client_->InjectExternalScriptControllerForTesting(
        std::move(external_script_controller));
  }
  TestApcClientImpl* apc_client() { return test_apc_client_; }
  MockApcOnboardingCoordinator* coordinator() { return coordinator_ref_; }
  autofill_assistant::MockExternalScriptController*
  external_script_controller() {
    return external_script_controller_ref_;
  }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  // Supporting members to create the testing environment.
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<MockApcOnboardingCoordinator> coordinator_ref_;
  raw_ptr<autofill_assistant::MockExternalScriptController>
      external_script_controller_ref_;
  // The object that is tested.
  raw_ptr<TestApcClientImpl> test_apc_client_;
};

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_Success) {
  raw_ptr<ApcClient> client =
      ApcClient::GetOrCreateForWebContents(web_contents());

  // There is one client per WebContents.
  EXPECT_EQ(client, apc_client());

  // The `ApcClient` is paused.
  EXPECT_FALSE(client->IsRunning());

  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .WillOnce(MoveArg<0>(&coordinator_callback));

  EXPECT_TRUE(client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/false));
  EXPECT_TRUE(client->IsRunning());

  // We cannot start a second flow.
  EXPECT_FALSE(client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/false));

  // Prepare to extract the callback to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::ExternalScriptController::ScriptResult)>
      external_script_controller_callback;
  EXPECT_CALL(*external_script_controller(), StartScript)
      .Times(1)
      .WillOnce(MoveArg<1>(&external_script_controller_callback));

  // Successful onboarding.
  std::move(coordinator_callback).Run(true);
  EXPECT_TRUE(client->IsRunning());

  autofill_assistant::ExternalScriptController::ScriptResult script_result = {
      /* success= */ true};
  std::move(external_script_controller_callback).Run(script_result);
  EXPECT_FALSE(client->IsRunning());
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_fromSettings) {
  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .WillOnce(MoveArg<0>(&coordinator_callback));

  EXPECT_TRUE(
      apc_client()->Start(GURL(kUrl1), kUsername1, /*skip_login=*/false));

  // Prepare to extract the script_params to the external script
  // controller.
  base::flat_map<std::string, std::string> params_map;
  EXPECT_CALL(*external_script_controller(), StartScript)
      .Times(1)
      .WillOnce(MoveArg<0>(&params_map));

  // Successful onboarding.
  std::move(coordinator_callback).Run(true);
  EXPECT_TRUE(apc_client()->IsRunning());
  EXPECT_THAT(params_map[kPasswordChangeSkipLoginParameter], StrEq("false"));
  EXPECT_THAT(params_map[kSourceParameter],
              StrEq(kSourcePasswordChangeSettings));
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_fromLeakWarning) {
  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .Times(1)
      .WillOnce(MoveArg<0>(&coordinator_callback));

  // `skip_login` equals to a trigger from leak warning.
  EXPECT_TRUE(
      apc_client()->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true));

  // Prepare to extract the script_params to the external script
  // controller.
  base::flat_map<std::string, std::string> params_map;
  EXPECT_CALL(*external_script_controller(), StartScript)
      .Times(1)
      .WillOnce(MoveArg<0>(&params_map));

  // Successful onboarding.
  std::move(coordinator_callback).Run(true);
  EXPECT_THAT(params_map[kPasswordChangeSkipLoginParameter], StrEq("true"));
  EXPECT_THAT(params_map[kSourceParameter],
              StrEq(kSourcePasswordChangeLeakWarning));
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_WithFailedOnboarding) {
  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .Times(1)
      .WillOnce(MoveArg<0>(&coordinator_callback));

  EXPECT_TRUE(
      apc_client()->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true));

  // Fail onboarding.
  std::move(coordinator_callback).Run(false);
  EXPECT_FALSE(apc_client()->IsRunning());
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_WithUnifiedSidePanelDisabled) {
  base::test::ScopedFeatureList override_feature_list;
  override_feature_list.InitWithFeatures({}, {features::kUnifiedSidePanel});
  raw_ptr<ApcClient> client =
      ApcClient::GetOrCreateForWebContents(web_contents());

  // There is one client per WebContents.
  EXPECT_EQ(client, apc_client());

  // The `ApcClient` is paused.
  EXPECT_FALSE(client->IsRunning());

  // Starting it does not work.
  EXPECT_FALSE(client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true));
  EXPECT_FALSE(client->IsRunning());
}
