// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill_assistant/password_change/apc_external_action_delegate.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"
#include "chrome/browser/autofill_assistant/password_change/mock_apc_onboarding_coordinator.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_apc_scrim_manager.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_side_panel_coordinator.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_stopped_bubble_coordinator.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill_assistant/browser/public/mock_headless_script_controller.h"
#include "components/autofill_assistant/browser/public/mock_runtime_manager.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/public/prefs.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_client_helper.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

constexpr char kUrl1[] = "https://www.example.com";
constexpr char kUsername1[] = "Lori";
constexpr char kDebugBundleId[] = "testuser/123/password_change/example.com";
constexpr char kDebugSocketId[] = "testuser";

constexpr char kPasswordChangeSkipLoginParameter[] =
    "PASSWORD_CHANGE_SKIP_LOGIN";
constexpr char kSourceParameter[] = "SOURCE";
constexpr char kDebugBundleIdParameter[] = "DEBUG_BUNDLE_ID";
constexpr char kDebugSocketIdParameter[] = "DEBUG_SOCKET_ID";
constexpr char kSourcePasswordChangeLeakWarning[] = "10";
constexpr char kSourcePasswordChangeSettings[] = "11";

constexpr int kDescriptionId1 = 3;
constexpr int kDescriptionId2 = 17;

class MockApcExternalActionDelegate : public ApcExternalActionDelegate {
 public:
  MockApcExternalActionDelegate(
      content::WebContents* web_contents,
      AssistantDisplayDelegate* display_delegate,
      ApcScrimManager* apc_scrim_manager,
      autofill_assistant::WebsiteLoginManager* website_login_manager)
      : ApcExternalActionDelegate(web_contents,
                                  display_delegate,
                                  apc_scrim_manager,
                                  website_login_manager) {}
  ~MockApcExternalActionDelegate() override = default;

  MOCK_METHOD(void, ShowStartingScreen, (const GURL&), (override));
  MOCK_METHOD(void, ShowCompletionScreen, (base::RepeatingClosure), (override));
  MOCK_METHOD(void, ShowErrorScreen, (), (override));
  MOCK_METHOD(void, SetupDisplay, (), (override));
  MOCK_METHOD(bool, PasswordWasSuccessfullyChanged, (), (override));
};

}  // namespace

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;

class TestApcClientImpl : public ApcClientImpl {
 public:
  static TestApcClientImpl* CreateForWebContents(
      content::WebContents* web_contents);

  explicit TestApcClientImpl(content::WebContents* web_contents)
      : ApcClientImpl(web_contents) {}

  std::unique_ptr<ApcOnboardingCoordinator> CreateOnboardingCoordinator()
      override {
    return std::move(onboarding_coordinator_);
  }

  std::unique_ptr<AssistantStoppedBubbleCoordinator>
  CreateAssistantStoppedBubbleCoordinator() override {
    if (!assistant_stopped_bubble_coordinator_) {
      ADD_FAILURE() << "assistant_stopped_bubble_coordinator_ is undefined, "
                       "please inject an instance of it.";
    }
    return std::move(assistant_stopped_bubble_coordinator_);
  }

  std::unique_ptr<AssistantSidePanelCoordinator> CreateSidePanel() override {
    return std::move(side_panel_);
  }

  std::unique_ptr<autofill_assistant::HeadlessScriptController>
  CreateHeadlessScriptController() override {
    return std::move(external_script_controller_);
  }

  autofill_assistant::RuntimeManager* GetRuntimeManager() override {
    return runtime_manager_;
  }

  std::unique_ptr<ApcScrimManager> CreateApcScrimManager() override {
    return std::move(scrim_manager_);
  }

  std::unique_ptr<ApcExternalActionDelegate> CreateApcExternalActionDelegate()
      override {
    return std::move(apc_external_action_delegate_);
  }

  std::unique_ptr<autofill_assistant::WebsiteLoginManager>
  CreateWebsiteLoginManager() override {
    return std::move(website_login_manager_);
  }

  password_manager::PasswordManagerClient* GetPasswordManagerClient() override {
    return password_manager_client_.get();
  }

  // Allows setting an onboarding coordinator that is returned by the factory
  // function. Must be called at least once before every expected call to
  // `CreateOnboardingCoordinator()`.
  void InjectOnboardingCoordinatorForTesting(
      std::unique_ptr<ApcOnboardingCoordinator> coordinator) {
    onboarding_coordinator_ = std::move(coordinator);
  }

  void InjectSidePanelForTesting(
      std::unique_ptr<AssistantSidePanelCoordinator> side_panel) {
    side_panel_ = std::move(side_panel);
  }

  // Allows setting an HeadlessScriptController. Must be called at least once
  // before every expected call to `CreateHeadlessScriptController()`.
  void InjectHeadlessScriptControllerForTesting(
      std::unique_ptr<autofill_assistant::HeadlessScriptController>
          external_script_controller) {
    external_script_controller_ = std::move(external_script_controller);
  }

  // Allows setting an RunTimeManager.
  void InjectRunTimeManagerForTesting(
      autofill_assistant::RuntimeManager* runtime_manager) {
    runtime_manager_ = runtime_manager;
  }

  // Allows setting an ApcScrimManager.
  void InjectApcScrimManagerForTesting(
      std::unique_ptr<ApcScrimManager> scrim_manager) {
    scrim_manager_ = std::move(scrim_manager);
  }

  // Must be called before every expected call to
  // CreateAssistantStoppedBubbleCoordinator().
  void InjectAssistantStoppedBubbleCoordinatorForTesting(
      std::unique_ptr<AssistantStoppedBubbleCoordinator>
          assistant_stopped_bubble_coordinator) {
    assistant_stopped_bubble_coordinator_ =
        std::move(assistant_stopped_bubble_coordinator);
  }

  // Allows setting a ApcExternalActionDelegate.
  void InjectApcExternalActionDelegateForTesting(
      std::unique_ptr<ApcExternalActionDelegate> apc_external_action_delegate) {
    apc_external_action_delegate_ = std::move(apc_external_action_delegate);
  }

  // Allows setting a WebsiteLoginManager.
  void InjectWebsiteLoginManagerForTesting(
      std::unique_ptr<autofill_assistant::WebsiteLoginManager>
          website_login_manager) {
    website_login_manager_ = std::move(website_login_manager);
  }

  // Allows setting an PasswordManagerClient.
  void InjectPasswordManagerClientForTesting(
      std::unique_ptr<password_manager::PasswordManagerClient>
          password_manager_client) {
    password_manager_client_ = std::move(password_manager_client);
  }

 private:
  std::unique_ptr<ApcOnboardingCoordinator> onboarding_coordinator_;
  std::unique_ptr<AssistantSidePanelCoordinator> side_panel_;
  std::unique_ptr<autofill_assistant::HeadlessScriptController>
      external_script_controller_;
  raw_ptr<autofill_assistant::RuntimeManager> runtime_manager_;
  std::unique_ptr<ApcScrimManager> scrim_manager_;
  std::unique_ptr<AssistantStoppedBubbleCoordinator>
      assistant_stopped_bubble_coordinator_;
  std::unique_ptr<ApcExternalActionDelegate> apc_external_action_delegate_;
  std::unique_ptr<autofill_assistant::WebsiteLoginManager>
      website_login_manager_;
  std::unique_ptr<password_manager::PasswordManagerClient>
      password_manager_client_;
};

// static
TestApcClientImpl* TestApcClientImpl::CreateForWebContents(
    content::WebContents* web_contents) {
  const void* key = WebContentsUserData<ApcClientImpl>::UserDataKey();
  web_contents->SetUserData(key,
                            std::make_unique<TestApcClientImpl>(web_contents));
  return static_cast<TestApcClientImpl*>(web_contents->GetUserData(key));
}

class ApcClientImplTest : public ChromeRenderViewHostTestHarness {
 public:
  ApcClientImplTest() {
    feature_list_.InitWithFeatures({features::kUnifiedSidePanel}, {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    test_apc_client_ = TestApcClientImpl::CreateForWebContents(web_contents());

    // Prepare the coordinator.
    auto coordinator = std::make_unique<MockApcOnboardingCoordinator>();
    onboarding_coordinator_ref_ = coordinator.get();
    test_apc_client_->InjectOnboardingCoordinatorForTesting(
        std::move(coordinator));

    // Prepare the side panel.
    auto side_panel = std::make_unique<MockAssistantSidePanelCoordinator>();
    side_panel_ref_ = side_panel.get();
    test_apc_client_->InjectSidePanelForTesting(std::move(side_panel));

    // Register the observer of the side panel. During testing, we implicitly
    // assume that there is only one.
    ON_CALL(*side_panel_ref_, AddObserver)
        .WillByDefault(SaveArg<0>(&side_panel_observer_));

    // Prepare the HeadlessScriptController.
    auto external_script_controller =
        std::make_unique<autofill_assistant::MockHeadlessScriptController>();
    external_script_controller_ref_ = external_script_controller.get();
    test_apc_client_->InjectHeadlessScriptControllerForTesting(
        std::move(external_script_controller));

    // Prepare the RunTimeManager.
    test_apc_client_->InjectRunTimeManagerForTesting(
        mock_runtime_manager_.get());

    // Prepare the ApcScrimManager.
    auto scrim_manager = std::make_unique<MockApcScrimManager>();
    scrim_manager_ref_ = scrim_manager.get();
    test_apc_client_->InjectApcScrimManagerForTesting(std::move(scrim_manager));

    // Prepare the AssistantStoppedBubbleCoordinator.
    auto assistant_stopped_bubble_coordinator =
        std::make_unique<NiceMock<MockAssistantStoppedBubbleCoordinator>>();
    assistant_stopped_bubble_coordinator_ref_ =
        assistant_stopped_bubble_coordinator.get();
    test_apc_client_->InjectAssistantStoppedBubbleCoordinatorForTesting(
        std::move(assistant_stopped_bubble_coordinator));

    // Prepare the PasswordManagerClient.
    auto password_manager_client =
        std::make_unique<password_manager::StubPasswordManagerClient>();
    password_manager_client_ref_ = password_manager_client.get();
    test_apc_client_->InjectPasswordManagerClientForTesting(
        std::move(password_manager_client));

    // Prepare the WebsiteLoginManager.
    auto website_login_manager =
        std::make_unique<autofill_assistant::MockWebsiteLoginManager>();
    website_login_manager_ref_ = website_login_manager.get();
    test_apc_client_->InjectWebsiteLoginManagerForTesting(
        std::move(website_login_manager));

    // Prepare the ApcExternalActionDelegate.
    auto apc_external_action_delegate =
        std::make_unique<MockApcExternalActionDelegate>(
            web_contents(), side_panel_ref_, scrim_manager_ref_,
            website_login_manager_ref_);
    apc_external_action_delegate_ref_ = apc_external_action_delegate.get();
    // As default, assume a password change run as successful.
    ON_CALL(*apc_external_action_delegate_ref_, PasswordWasSuccessfullyChanged)
        .WillByDefault(Return(true));
    test_apc_client_->InjectApcExternalActionDelegateForTesting(
        std::move(apc_external_action_delegate));
  }

  TestApcClientImpl* apc_client() { return test_apc_client_; }
  MockApcOnboardingCoordinator* coordinator() {
    return onboarding_coordinator_ref_;
  }
  MockAssistantSidePanelCoordinator* side_panel() { return side_panel_ref_; }
  MockApcScrimManager* scrim_manager() { return scrim_manager_ref_; }
  MockAssistantStoppedBubbleCoordinator*
  assistant_stopped_bubble_coordinator() {
    return assistant_stopped_bubble_coordinator_ref_;
  }
  MockApcExternalActionDelegate* apc_external_action_delegate() {
    return apc_external_action_delegate_ref_;
  }
  AssistantSidePanelCoordinator::Observer* side_panel_observer() {
    return side_panel_observer_;
  }
  autofill_assistant::MockHeadlessScriptController*
  external_script_controller() {
    return external_script_controller_ref_;
  }
  autofill_assistant::MockRuntimeManager* runtime_manager() {
    return mock_runtime_manager_.get();
  }

  PrefService* GetPrefs() { return profile()->GetPrefs(); }

 private:
  // Necessary to turn on the unified sidepanel.
  base::test::ScopedFeatureList feature_list_;

  // Pointers to mocked components that are injected into the `ApcClientImpl`.
  raw_ptr<MockApcOnboardingCoordinator> onboarding_coordinator_ref_ = nullptr;
  raw_ptr<MockAssistantSidePanelCoordinator> side_panel_ref_ = nullptr;
  raw_ptr<autofill_assistant::MockHeadlessScriptController>
      external_script_controller_ref_ = nullptr;
  raw_ptr<MockApcScrimManager> scrim_manager_ref_ = nullptr;
  raw_ptr<MockAssistantStoppedBubbleCoordinator>
      assistant_stopped_bubble_coordinator_ref_ = nullptr;
  raw_ptr<MockApcExternalActionDelegate> apc_external_action_delegate_ref_ =
      nullptr;
  raw_ptr<password_manager::StubPasswordManagerClient>
      password_manager_client_ref_ = nullptr;
  raw_ptr<autofill_assistant::WebsiteLoginManager> website_login_manager_ref_ =
      nullptr;

  // The last registered side panel observer - may be null or dangling.
  raw_ptr<AssistantSidePanelCoordinator::Observer> side_panel_observer_ =
      nullptr;

  // The object that is tested.
  raw_ptr<TestApcClientImpl> test_apc_client_ = nullptr;
  std::unique_ptr<autofill_assistant::MockRuntimeManager>
      mock_runtime_manager_ =
          std::make_unique<autofill_assistant::MockRuntimeManager>();
};

TEST_F(ApcClientImplTest,
       CreateAndStartApcFlow_Success_PasswordSuccessfullyChanged) {
  raw_ptr<ApcClient> client =
      ApcClient::GetOrCreateForWebContents(web_contents());

  // There is one client per WebContents.
  EXPECT_EQ(client, apc_client());

  // The `ApcClient` is paused.
  EXPECT_FALSE(client->IsRunning());

  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback onboarding_coordinator_callback;
  base::MockCallback<ApcClient::ResultCallback> result_callback1,
      result_callback2;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .WillOnce(MoveArg<0>(&onboarding_coordinator_callback));
  EXPECT_CALL(*runtime_manager(),
              SetUIState(autofill_assistant::UIState::kShown));
  EXPECT_CALL(*apc_external_action_delegate(), ShowStartingScreen(GURL(kUrl1)));
  EXPECT_CALL(*scrim_manager(), Show());
  EXPECT_CALL(*assistant_stopped_bubble_coordinator(), Show()).Times(0);

  client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/false,
                result_callback1.Get());

  EXPECT_TRUE(client->IsRunning());

  // We cannot start a second flow.
  EXPECT_CALL(result_callback2, Run(false));
  client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/false,
                result_callback2.Get(),
                /*debug_run_information=*/absl::nullopt);

  // Prepare to extract the callback to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::HeadlessScriptController::ScriptResult)>
      external_script_controller_callback;
  EXPECT_CALL(*external_script_controller(), StartScript(_, _))
      .Times(1)
      .WillOnce(MoveArg<1>(&external_script_controller_callback));

  // Successful onboarding.
  std::move(onboarding_coordinator_callback).Run(true);
  EXPECT_TRUE(client->IsRunning());

  autofill_assistant::HeadlessScriptController::ScriptResult script_result = {
      .success = true};

  EXPECT_CALL(*runtime_manager(),
              SetUIState(autofill_assistant::UIState::kNotShown));
  EXPECT_CALL(result_callback1, Run(true));

  // Prepare to extract the callback from the completion screen call.
  base::RepeatingClosure show_completion_screen_callback;
  EXPECT_CALL(*apc_external_action_delegate(), ShowCompletionScreen(_))
      .Times(1)
      .WillOnce(MoveArg<0>(&show_completion_screen_callback));
  EXPECT_CALL(*side_panel(), Die);

  std::move(external_script_controller_callback).Run(script_result);
  std::move(show_completion_screen_callback).Run();

  EXPECT_FALSE(client->IsRunning());
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_Success_PasswordWasNotChanged) {
  raw_ptr<ApcClient> client =
      ApcClient::GetOrCreateForWebContents(web_contents());

  ON_CALL(*apc_external_action_delegate(), PasswordWasSuccessfullyChanged())
      .WillByDefault(Return(false));

  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback onboarding_coordinator_callback;
  base::MockCallback<ApcClient::ResultCallback> result_callback1,
      result_callback2;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .WillOnce(MoveArg<0>(&onboarding_coordinator_callback));

  client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/false,
                result_callback1.Get());

  // Prepare to extract the callback to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::HeadlessScriptController::ScriptResult)>
      external_script_controller_callback;
  EXPECT_CALL(*external_script_controller(), StartScript(_, _))
      .Times(1)
      .WillOnce(MoveArg<1>(&external_script_controller_callback));

  // Successful onboarding.
  std::move(onboarding_coordinator_callback).Run(true);

  autofill_assistant::HeadlessScriptController::ScriptResult script_result = {
      .success = true};

  EXPECT_CALL(*apc_external_action_delegate(), ShowErrorScreen()).Times(0);
  EXPECT_CALL(*apc_external_action_delegate(), ShowCompletionScreen(_))
      .Times(0);
  EXPECT_CALL(*side_panel(), Die);

  std::move(external_script_controller_callback).Run(script_result);
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_ScriptFails) {
  raw_ptr<ApcClient> client =
      ApcClient::GetOrCreateForWebContents(web_contents());

  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback onboarding_coordinator_callback;
  base::MockCallback<ApcClient::ResultCallback> result_callback1,
      result_callback2;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .WillOnce(MoveArg<0>(&onboarding_coordinator_callback));

  client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/false,
                result_callback1.Get());

  // Prepare to extract the callback to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::HeadlessScriptController::ScriptResult)>
      external_script_controller_callback;
  EXPECT_CALL(*external_script_controller(), StartScript(_, _))
      .Times(1)
      .WillOnce(MoveArg<1>(&external_script_controller_callback));

  // Successful onboarding.
  std::move(onboarding_coordinator_callback).Run(true);

  autofill_assistant::HeadlessScriptController::ScriptResult script_result = {
      .success = false};

  EXPECT_CALL(*apc_external_action_delegate(), ShowErrorScreen());

  std::move(external_script_controller_callback).Run(script_result);
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_fromSettings) {
  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .WillOnce(MoveArg<0>(&coordinator_callback));

  apc_client()->Start(GURL(kUrl1), kUsername1, /*skip_login=*/false,
                      /*callback=*/base::DoNothing(),
                      /*debug_run_information=*/absl::nullopt);

  // Prepare to extract the script_params to the external script
  // controller.
  base::flat_map<std::string, std::string> params_map;
  EXPECT_CALL(*external_script_controller(), StartScript(_, _))
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

  // `skip_login = true` equals a trigger from leak warning.
  apc_client()->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true,
                      /*callback=*/base::DoNothing(),
                      /*debug_run_information=*/absl::nullopt);

  // Prepare to extract the script_params to the external script
  // controller.
  base::flat_map<std::string, std::string> params_map;
  EXPECT_CALL(*external_script_controller(), StartScript(_, _))
      .Times(1)
      .WillOnce(MoveArg<0>(&params_map));

  // Successful onboarding.
  std::move(coordinator_callback).Run(true);
  EXPECT_THAT(params_map[kPasswordChangeSkipLoginParameter], StrEq("true"));
  EXPECT_THAT(params_map[kSourceParameter],
              StrEq(kSourcePasswordChangeLeakWarning));
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_withDebugInformation) {
  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .Times(1)
      .WillOnce(MoveArg<0>(&coordinator_callback));

  apc_client()->Start(
      GURL(kUrl1), kUsername1, /*skip_login=*/false,
      /*callback=*/base::DoNothing(),
      ApcClient::DebugRunInformation{.bundle_id = kDebugBundleId,
                                     .socket_id = kDebugSocketId});

  // Prepare to extract the script_params to the external script
  // controller.
  base::flat_map<std::string, std::string> params_map;
  EXPECT_CALL(*external_script_controller(), StartScript(_, _))
      .Times(1)
      .WillOnce(MoveArg<0>(&params_map));

  // Successful onboarding.
  std::move(coordinator_callback).Run(true);
  EXPECT_EQ(params_map[kDebugBundleIdParameter], kDebugBundleId);
  EXPECT_EQ(params_map[kDebugSocketIdParameter], kDebugSocketId);
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_WithFailedOnboarding) {
  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .Times(1)
      .WillOnce(MoveArg<0>(&coordinator_callback));

  apc_client()->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true,
                      /*callback=*/base::DoNothing(),
                      /*debug_run_information=*/absl::nullopt);

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

  EXPECT_CALL(*runtime_manager(),
              SetUIState(autofill_assistant::UIState::kShown))
      .Times(0);

  // Starting it does not work.
  client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true,
                /*callback=*/base::DoNothing(),
                /*debug_run_information=*/absl::nullopt);
  EXPECT_FALSE(client->IsRunning());
}

TEST_F(ApcClientImplTest, CreateAndStartApcFlow_WithAutofillAssistantDisabled) {
  GetPrefs()->SetBoolean(autofill_assistant::prefs::kAutofillAssistantEnabled,
                         false);
  raw_ptr<ApcClient> client =
      ApcClient::GetOrCreateForWebContents(web_contents());

  // There is one client per WebContents.
  EXPECT_EQ(client, apc_client());

  // The `ApcClient` is paused.
  EXPECT_FALSE(client->IsRunning());

  EXPECT_CALL(*runtime_manager(),
              SetUIState(autofill_assistant::UIState::kShown))
      .Times(0);

  base::MockCallback<ApcClient::ResultCallback> result_callback;
  EXPECT_CALL(result_callback, Run(false));

  // Starting it does not work.
  client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true,
                /*callback=*/result_callback.Get(),
                /*debug_run_information=*/absl::nullopt);
  EXPECT_FALSE(client->IsRunning());
}

TEST_F(ApcClientImplTest,
       CreateAndStartApcFlow_WithoutPasswordClientManagerFlowStops) {
  apc_client()->InjectPasswordManagerClientForTesting(nullptr);

  apc_client()->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true,
                      /*callback=*/base::DoNothing(),
                      /*debug_run_information=*/absl::nullopt);

  // Fail run.
  EXPECT_FALSE(apc_client()->IsRunning());
}

TEST_F(ApcClientImplTest, StopApcFlow) {
  raw_ptr<ApcClient> client =
      ApcClient::GetOrCreateForWebContents(web_contents());

  base::MockCallback<ApcClient::ResultCallback> result_callback;

  client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true,
                result_callback.Get(), /*debug_run_information=*/absl::nullopt);

  // Calling `Stop()` twice only triggers the callback the first time around.
  EXPECT_CALL(result_callback, Run(false)).Times(1);
  client->Stop();
  client->Stop();
}

TEST_F(ApcClientImplTest, OnHidden_WithOngoingApcFlow) {
  ASSERT_FALSE(side_panel_observer());

  // Prepare to extract the callback to the coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .Times(1)
      .WillOnce(MoveArg<0>(&coordinator_callback));
  EXPECT_CALL(*runtime_manager(),
              SetUIState(autofill_assistant::UIState::kShown));
  apc_client()->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true,
                      /*callback=*/base::DoNothing(),
                      /*debug_run_information=*/absl::nullopt);
  std::move(coordinator_callback).Run(true);
  EXPECT_TRUE(apc_client()->IsRunning());

  // The `ApcClientImpl` is registered as an observer to the side panel.
  ASSERT_EQ(side_panel_observer(), apc_client());

  EXPECT_CALL(*runtime_manager(),
              SetUIState(autofill_assistant::UIState::kNotShown));
  EXPECT_CALL(*assistant_stopped_bubble_coordinator(), Show());
  // Simulate hiding the side panel.
  side_panel_observer()->OnHidden();

  EXPECT_FALSE(apc_client()->IsRunning());
}

TEST_F(ApcClientImplTest, PromptForConsent) {
  // `ApcClient` should forward the consent request to the onboarding
  // coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(*coordinator(), PerformOnboarding)
      .Times(1)
      .WillOnce(MoveArg<0>(&coordinator_callback));

  base::MockCallback<ApcClient::OnboardingResultCallback> result_callback;
  apc_client()->PromptForConsent(result_callback.Get());
  EXPECT_TRUE(apc_client()->IsRunning());

  EXPECT_CALL(result_callback, Run(true));
  std::move(coordinator_callback).Run(true);
  EXPECT_FALSE(apc_client()->IsRunning());
}

TEST_F(ApcClientImplTest, RevokeConsent) {
  // `ApcClient` should forward the consent revokation to the onboarding
  // coordinator.
  ApcOnboardingCoordinator::Callback coordinator_callback;
  EXPECT_CALL(
      *coordinator(),
      RevokeConsent(std::vector<int>({kDescriptionId1, kDescriptionId2})));

  apc_client()->RevokeConsent({kDescriptionId1, kDescriptionId2});
}
