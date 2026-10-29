// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#endif

namespace contextual_tasks {

std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TestSigninClient>(profile->GetPrefs());
}

lens::ClientToSearchMessage CreateInjectChromeInputMessage(
    lens::ClientToSearchMessage::InjectChromeInput::InputType input_type,
    bool is_active) {
  lens::ClientToSearchMessage message;
  auto* inject_input = message.mutable_inject_chrome_input();
  inject_input->set_input_type(input_type);
  inject_input->set_is_active(is_active);
  return message;
}

class ContextualTasksWebContentsUserDataTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactories(IdentityTestEnvironmentProfileAdaptor::
                                    GetIdentityTestEnvironmentFactories());
    builder.AddTestingFactory(ChromeSigninClientFactory::GetInstance(),
                              base::BindRepeating(&BuildTestSigninClient));
    profile_ = builder.Build();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    auto* signin_client = static_cast<TestSigninClient*>(
        identity_test_env_adaptor_->identity_test_env()->signin_client());
    identity_test_env_adaptor_->identity_test_env()->SetTestURLLoaderFactory(
        signin_client->GetTestURLLoaderFactory());

    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    web_contents_ = web_contents_factory_->CreateWebContents(profile_.get());
  }

  void TearDown() override {
    web_contents_ = nullptr;
    web_contents_factory_.reset();
    identity_test_env_adaptor_.reset();
    profile_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> identity_test_env_adaptor_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
};

TEST_F(ContextualTasksWebContentsUserDataTest, GetOrCreate) {
  // Initially null.
  EXPECT_FALSE(
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_));

  // Create it.
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  EXPECT_TRUE(user_data);

  // Get it again.
  auto* user_data2 =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  EXPECT_EQ(user_data, user_data2);
}

TEST_F(ContextualTasksWebContentsUserDataTest, SetAndGetModel) {
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  EXPECT_FALSE(user_data->input_state_model());

  auto mock_handle =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();
  omnibox::SearchboxConfig config;
  auto input_state_model = std::make_unique<contextual_search::InputStateModel>(
      *mock_handle, config, GURL(), false, false, false);

  auto weak_ptr = input_state_model->AsWeakPtr();

  user_data->set_input_state_model(std::move(input_state_model));
  EXPECT_EQ(user_data->input_state_model().get(), weak_ptr.get());
}

class FakeUiService : public ContextualTasksUiService {
 public:
  explicit FakeUiService(Profile* profile)
      : ContextualTasksUiService(
            profile,
            /*delegate=*/nullptr,
            /*contextual_tasks_service=*/nullptr,
            /*identity_manager=*/nullptr,
            /*aim_eligibility_service=*/nullptr,
            /*eligibility_manager=*/nullptr,
            /*cookie_synchronizer=*/nullptr) {}

  bool IsSignedInToBrowserWithValidCredentials() override { return true; }
  bool IsUrlForPrimaryAccount(const GURL& url) override { return true; }
};

TEST_F(ContextualTasksWebContentsUserDataTest,
       GetOrCreateInputStateModel_UiServicePath) {
  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<FakeUiService>(
            Profile::FromBrowserContext(context));
      }));

  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  auto mock_handle =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();

  auto model_weak = user_data->GetOrCreateInputStateModel(*mock_handle);
  ASSERT_TRUE(model_weak);
  EXPECT_TRUE(model_weak->browser_identity_matches_aim_identity_for_testing());

  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::NullCallback());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       GetOrCreateInputStateModel_IdentityFallback_DefaultTrue) {
  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext*) {
        return std::unique_ptr<KeyedService>();
      }));

  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_adaptor_->identity_test_env()->SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});

  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  auto mock_handle =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();

  auto model_weak = user_data->GetOrCreateInputStateModel(*mock_handle);
  ASSERT_TRUE(model_weak);
  EXPECT_TRUE(model_weak->browser_identity_matches_aim_identity_for_testing());

  // Reset testing factory to prevent dangling references to stack variables.
  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::NullCallback());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       GetOrCreateInputStateModel_IdentityFallback_FalseBypassesIdentityManager) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kComposeboxDriveContextMenuOption,
      {{"enable_identity_fallback", "false"}});

  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext*) {
        return std::unique_ptr<KeyedService>();
      }));

  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_adaptor_->identity_test_env()->SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});

  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  auto mock_handle =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();

  auto model_weak = user_data->GetOrCreateInputStateModel(*mock_handle);
  ASSERT_TRUE(model_weak);
  EXPECT_FALSE(model_weak->browser_identity_matches_aim_identity_for_testing());

  // Reset testing factory to prevent dangling references to stack variables.
  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::NullCallback());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       GetOrCreateInputStateModel_MultipleSessions) {
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  auto mock_handle_a =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();
  auto mock_handle_b =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();

  ASSERT_NE(mock_handle_a->session_id(), mock_handle_b->session_id());

  // Create model for Session A
  auto model_a_weak = user_data->GetOrCreateInputStateModel(*mock_handle_a);
  ASSERT_TRUE(model_a_weak);

  // Create model for Session B. Model A should not be destroyed.
  auto model_b_weak = user_data->GetOrCreateInputStateModel(*mock_handle_b);
  ASSERT_TRUE(model_b_weak);
  EXPECT_TRUE(model_a_weak);

  // Retrieve Model A again. Verify it returns the existing Model A instance.
  auto model_a_weak_2 = user_data->GetOrCreateInputStateModel(*mock_handle_a);
  EXPECT_EQ(model_a_weak.get(), model_a_weak_2.get());

  // Destroy Session A. It should be garbage collected on next GetOrCreate call.
  mock_handle_a.reset();

  auto mock_handle_c =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();
  user_data->GetOrCreateInputStateModel(*mock_handle_c);

  // Model A weak pointer should now be null (deleted from map).
  EXPECT_FALSE(model_a_weak);
}

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    GetOrCreateInputStateModel_UpdatesStaleEmptyConfigWhenValidConfigAvailable) {
  // Prepare a valid searchbox config containing active tool definitions.
  omnibox::SearchboxConfig valid_config;
  valid_config.mutable_rule_set();
  auto* tool = valid_config.add_tool_configs();
  tool->set_tool(omnibox::TOOL_MODE_DEEP_SEARCH);
  // Set up MockAimEligibilityService to dynamically return `current_config`.
  const omnibox::SearchboxConfig* current_config = nullptr;
  AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating(
          [](const omnibox::SearchboxConfig** config_ptr,
             content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> {
            auto mock =
                std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
                    *Profile::FromBrowserContext(context)->GetPrefs(),
                    /*template_url_service=*/nullptr,
                    /*url_loader_factory=*/nullptr,
                    /*identity_manager=*/nullptr);
            ON_CALL(*mock, GetSearchboxConfig()).WillByDefault([config_ptr]() {
              return *config_ptr;
            });
            return mock;
          },
          &current_config));
  // Initialize WebContents user data and create a mock session handle.
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  auto mock_handle =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();

  // Initial creation with empty/null config: model has has_valid_config() ==
  // false.
  auto model_initial = user_data->GetOrCreateInputStateModel(*mock_handle);
  ASSERT_TRUE(model_initial);
  EXPECT_FALSE(model_initial->has_valid_config());
  // A non-empty searchbox configuration becomes available.
  current_config = &valid_config;

  // Calling `GetOrCreateInputStateModel` again should update the existing
  // cached model in-place rather than invalidating it, preserving
  // subscriptions.
  auto model_updated = user_data->GetOrCreateInputStateModel(*mock_handle);
  ASSERT_TRUE(model_updated);
  EXPECT_TRUE(model_updated->has_valid_config());
  // The existing instance is updated in-place without destroying weak pointers.
  EXPECT_TRUE(model_initial);
  EXPECT_EQ(model_initial.get(), model_updated.get());

  // Reset testing factory to prevent dangling references to stack variables.
  AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::NullCallback());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       GetOrCreateInputStateModel_UpdatesIdentityStateOnCacheHit) {
  class ConfigurableUiService : public FakeUiService {
   public:
    ConfigurableUiService(Profile* profile, bool* signed_in, bool* url_match)
        : FakeUiService(profile),
          signed_in_(signed_in),
          url_match_(url_match) {}
    bool IsSignedInToBrowserWithValidCredentials() override {
      return *signed_in_;
    }
    bool IsUrlForPrimaryAccount(const GURL& url) override {
      return *url_match_;
    }

   private:
    raw_ptr<bool> signed_in_;
    raw_ptr<bool> url_match_;
  };

  bool is_signed_in = true;
  bool is_url_for_primary = true;
  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating(
          [](bool* signed_in, bool* url_match, content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> {
            return std::make_unique<ConfigurableUiService>(
                Profile::FromBrowserContext(context), signed_in, url_match);
          },
          &is_signed_in, &is_url_for_primary));

  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  auto mock_handle =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();

  auto model_initial = user_data->GetOrCreateInputStateModel(*mock_handle);
  ASSERT_TRUE(model_initial);
  EXPECT_TRUE(model_initial->is_signed_in_for_testing());
  EXPECT_TRUE(
      model_initial->browser_identity_matches_aim_identity_for_testing());

  // Simulate signing out and retrieving the cached InputStateModel again.
  is_signed_in = false;
  is_url_for_primary = false;
  auto model_updated = user_data->GetOrCreateInputStateModel(*mock_handle);
  ASSERT_TRUE(model_updated);
  EXPECT_EQ(model_initial.get(), model_updated.get());
  EXPECT_FALSE(model_updated->is_signed_in_for_testing());
  EXPECT_FALSE(
      model_updated->browser_identity_matches_aim_identity_for_testing());

  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::NullCallback());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       MultipleExtensionFrames_PerFrameHandshakeAndTargetFrameRouting) {
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_TRUE(user_data);

  const int kFrame1Id = 1;
  const int kFrame2Id = 2;
  const void* frame1_id = &kFrame1Id;
  const void* frame2_id = &kFrame2Id;

  int frame1_handshakes = 0;
  int frame2_handshakes = 0;
  std::vector<lens::ClientToSearchMessage> frame1_messages;
  std::vector<lens::ClientToSearchMessage> frame2_messages;

  user_data->RegisterExtensionFrame(frame1_id);
  user_data->UpdateExtensionFrameBound(
      frame1_id, /*is_page_bound=*/true,
      base::BindRepeating(
          [](std::vector<lens::ClientToSearchMessage>* msgs,
             const lens::ClientToSearchMessage& msg) { msgs->push_back(msg); },
          &frame1_messages),
      base::BindRepeating([](int* count) { ++(*count); }, &frame1_handshakes),
      base::DoNothing());

  user_data->RegisterExtensionFrame(frame2_id);
  user_data->UpdateExtensionFrameBound(
      frame2_id, /*is_page_bound=*/true,
      base::BindRepeating(
          [](std::vector<lens::ClientToSearchMessage>* msgs,
             const lens::ClientToSearchMessage& msg) { msgs->push_back(msg); },
          &frame2_messages),
      base::BindRepeating([](int* count) { ++(*count); }, &frame2_handshakes),
      base::DoNothing());

  // Before any handshake completes, outbound messages fall back to frame1
  // (the first bound frame).
  user_data->PostSearchMessage(CreateInjectChromeInputMessage(
      lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY,
      /*is_active=*/true));
  EXPECT_EQ(frame1_messages.size(), 1u);
  EXPECT_EQ(frame2_messages.size(), 0u);

  // Handshake response from frame1 should only notify frame1's handshake
  // callback, leaving frame2 free to complete its own handshake.
  lens::SearchToClientMessage handshake_msg;
  handshake_msg.mutable_handshake_response()->set_auth_user_index(0);
  std::vector<uint8_t> handshake_bytes(handshake_msg.ByteSizeLong());
  handshake_msg.SerializeToArray(handshake_bytes.data(),
                                 handshake_bytes.size());

  EXPECT_TRUE(user_data->OnSearchMessageReceived(handshake_bytes, frame1_id));
  EXPECT_EQ(frame1_handshakes, 1);
  EXPECT_EQ(frame2_handshakes, 0);

  // Now frame2 (e.g. context_library.html) completes its handshake. Only
  // frame2's handshake callback should fire, and frame2 becomes the active
  // target frame for outbound messages even while frame1 (e.g. tab_picker.html)
  // is still registered at index 0.
  EXPECT_TRUE(user_data->OnSearchMessageReceived(handshake_bytes, frame2_id));
  EXPECT_EQ(frame1_handshakes, 1);
  EXPECT_EQ(frame2_handshakes, 1);

  frame1_messages.clear();
  frame2_messages.clear();
  user_data->PostSearchMessage(CreateInjectChromeInputMessage(
      lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP,
      /*is_active=*/true));
  EXPECT_EQ(frame1_messages.size(), 0u);
  EXPECT_EQ(frame2_messages.size(), 1u);

  // If frame2 (e.g. tab_picker.html opened after context_library.html) is
  // unregistered while frame1 (e.g. context_library.html) remains bound and
  // handshake-complete, `UnregisterExtensionFrame` immediately re-sends
  // `OnContextStateChanged` through frame1 so Search's
  // `ChromePostMessageService.activeChromeWindow` switches back to frame1.
  frame1_messages.clear();
  frame2_messages.clear();
  user_data->UnregisterExtensionFrame(frame2_id);
  ASSERT_EQ(frame1_messages.size(), 1u);
  EXPECT_TRUE(frame1_messages[0].has_on_context_state_changed());
  EXPECT_EQ(frame2_messages.size(), 0u);

  frame1_messages.clear();
  user_data->PostSearchMessage(CreateInjectChromeInputMessage(
      lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP,
      /*is_active=*/false));
  EXPECT_EQ(frame1_messages.size(), 1u);
  EXPECT_EQ(frame2_messages.size(), 0u);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       OnSuggestInputsChanged_DisabledByDefault) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("suggest_session_disabled"));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> suggest_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage& message) {
        if (message.has_on_suggest_inputs_changed()) {
          suggest_messages.push_back(message);
        }
      }));

  base::UnguessableToken token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting().push_back(token);
  mock_handle->AttachTabPendingUpload(token, 10, GURL("https://example.com/1"),
                                      "Tab 1");
  contextual_search::FileInfo file_info;
  file_info.file_token = token;
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(101);
  file_info.request_id = req_id;
  std::vector<contextual_search::FileInfo> uploaded_files = {file_info};
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  user_data->OnTabContextUploadStarted(token);
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  EXPECT_TRUE(suggest_messages.empty());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       OnSuggestInputsChanged_ExplicitTabUploadAndRemovalLifecycle) {
  base::test::ScopedFeatureList feature_list{
      kContextualTasksTabBasedSuggestions};

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("suggest_session_123"));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> suggest_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage& message) {
        if (message.has_on_suggest_inputs_changed()) {
          suggest_messages.push_back(message);
        }
      }));

  // Attach Tab 1 and start uploading: no OnSuggestInputsChanged while in
  // flight.
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  contextual_search::FileInfo file1;
  file1.file_token = token1;
  lens::LensOverlayRequestId req1;
  req1.set_context_id(101);
  file1.request_id = req1;
  file1.input_data = std::make_unique<lens::ContextualInputData>();
  file1.input_data->upload_type = lens::LensOverlayContextualInputUploadType::
      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;

  std::vector<contextual_search::FileInfo> uploaded_files;
  uploaded_files.push_back(file1);
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  // Attaching the tab notifies `user_data` of the tab context change, which
  // updates its context library input state and suggest inputs.
  user_data->OnTabContextUploadStarted(token1);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token1);
  mock_handle->AttachTabPendingUpload(token1, 10, GURL("https://example.com/1"),
                                      "Tab 1");
  EXPECT_TRUE(suggest_messages.empty());

  // Tab 1 finishes uploading -> emits OnSuggestInputsChanged with Tab 1.
  user_data->OnContextUploadStatusChanged(
      token1, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  ASSERT_EQ(suggest_messages.size(), 1u);
  const auto& first_update =
      suggest_messages.back().on_suggest_inputs_changed();
  ASSERT_EQ(first_update.added_contexts_size(), 1);
  EXPECT_EQ(first_update.added_contexts(0).search_session_id(),
            "suggest_session_123");
  EXPECT_EQ(first_update.added_contexts(0).request_id().context_id(), 101);
  EXPECT_EQ(first_update.added_contexts(0).contextual_input_upload_type(),
            lens::LensOverlayContextualInputUploadType::
                CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);

  // Attach Tab 2 and start uploading: no redundant OnSuggestInputsChanged while
  // Tab 2 is still uploading.
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  contextual_search::FileInfo file2;
  file2.file_token = token2;
  lens::LensOverlayRequestId req2;
  req2.set_context_id(202);
  file2.request_id = req2;
  uploaded_files.push_back(file2);

  user_data->OnTabContextUploadStarted(token2);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token2);
  mock_handle->AttachTabPendingUpload(token2, 20, GURL("https://example.com/2"),
                                      "Tab 2");
  EXPECT_EQ(suggest_messages.size(), 1u);

  // Tab 2 finishes uploading -> emits OnSuggestInputsChanged with both tabs.
  user_data->OnContextUploadStatusChanged(
      token2, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  ASSERT_EQ(suggest_messages.size(), 2u);
  const auto& second_update =
      suggest_messages.back().on_suggest_inputs_changed();
  ASSERT_EQ(second_update.added_contexts_size(), 2);
  EXPECT_EQ(second_update.added_contexts(0).request_id().context_id(), 101);
  EXPECT_EQ(second_update.added_contexts(1).request_id().context_id(), 202);

  // Removing Tab 1 emits OnSuggestInputsChanged with only Tab 2.
  user_data->DeleteContext(token1);
  ASSERT_EQ(suggest_messages.size(), 3u);
  const auto& third_update =
      suggest_messages.back().on_suggest_inputs_changed();
  ASSERT_EQ(third_update.added_contexts_size(), 1);
  EXPECT_EQ(third_update.added_contexts(0).request_id().context_id(), 202);

  // Clearing all files emits an empty OnSuggestInputsChanged once.
  user_data->ClearFiles();
  ASSERT_EQ(suggest_messages.size(), 4u);
  EXPECT_EQ(
      suggest_messages.back().on_suggest_inputs_changed().added_contexts_size(),
      0);

  // Subsequent no-op updates do not emit duplicate empty messages.
  user_data->ClearFiles();
  EXPECT_EQ(suggest_messages.size(), 4u);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       OnSuggestInputsChanged_ImplicitAndDelayedTabsExcluded) {
  base::test::ScopedFeatureList feature_list{
      kContextualTasksTabBasedSuggestions};

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("suggest_session_implicit"));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> suggest_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage& message) {
        if (message.has_on_suggest_inputs_changed()) {
          suggest_messages.push_back(message);
        }
      }));

  // 1. Implicit upload (`is_implicit_upload = true`) must not emit suggest
  // inputs.
  base::UnguessableToken implicit_token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting().push_back(implicit_token);
  mock_handle->AttachTabPendingUpload(
      implicit_token, 10, GURL("https://example.com/implicit"), "Implicit Tab");
  contextual_search::FileInfo implicit_file;
  implicit_file.file_token = implicit_token;
  implicit_file.is_implicit_upload = true;
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(303);
  implicit_file.request_id = req_id;

  std::vector<contextual_search::FileInfo> uploaded_files = {implicit_file};
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  user_data->OnContextUploadStatusChanged(
      implicit_token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  EXPECT_TRUE(suggest_messages.empty());

  // 2. Delayed tab (`AddPendingDelayedTab`) must not emit suggest inputs.
  // Adding it notifies `user_data` of the tab context change.
  mock_handle->AddPendingDelayedTab(20, GURL("https://example.com/delayed"),
                                    "Delayed Tab");
  EXPECT_TRUE(suggest_messages.empty());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       OnSuggestInputsChanged_UploadFailureClearsAndRecoveryRestores) {
  base::test::ScopedFeatureList feature_list{
      kContextualTasksTabBasedSuggestions};

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("suggest_session_recovery"));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> suggest_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage& message) {
        if (message.has_on_suggest_inputs_changed()) {
          suggest_messages.push_back(message);
        }
      }));

  base::UnguessableToken token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting().push_back(token);
  mock_handle->AttachTabPendingUpload(token, 10, GURL("https://example.com/1"),
                                      "Tab 1");
  contextual_search::FileInfo file_info;
  file_info.file_token = token;
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(404);
  file_info.request_id = req_id;
  std::vector<contextual_search::FileInfo> uploaded_files = {file_info};
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  user_data->OnTabContextUploadStarted(token);
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  ASSERT_EQ(suggest_messages.size(), 1u);
  EXPECT_EQ(
      suggest_messages.back().on_suggest_inputs_changed().added_contexts_size(),
      1);

  // Expiry or failure removes the tab from suggest inputs.
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadExpired, std::nullopt);
  ASSERT_EQ(suggest_messages.size(), 2u);
  EXPECT_EQ(
      suggest_messages.back().on_suggest_inputs_changed().added_contexts_size(),
      0);

  // Re-upload completion restores the tab in suggest inputs.
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  ASSERT_EQ(suggest_messages.size(), 3u);
  ASSERT_EQ(
      suggest_messages.back().on_suggest_inputs_changed().added_contexts_size(),
      1);
  EXPECT_EQ(suggest_messages.back()
                .on_suggest_inputs_changed()
                .added_contexts(0)
                .request_id()
                .context_id(),
            404);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       OnTaskDisassociatedFromTab_RemovesContextAndUnmountsContextLibrary) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(task_id, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->SetTaskId(task_id);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> inject_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage& message) {
        if (message.has_inject_chrome_input() &&
            message.inject_chrome_input().input_type() ==
                lens::ClientToSearchMessage::InjectChromeInput::
                    CONTEXT_LIBRARY) {
          inject_messages.push_back(message);
        }
      }));

  // Attach Tab 10: mounts the context library button (`is_active = true`).
  const SessionID tab_id = SessionID::FromSerializedValue(10);
  base::UnguessableToken token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting().push_back(token);
  mock_handle->AttachTabPendingUpload(token, tab_id.id(),
                                      GURL("https://example.com/1"), "Tab 1");
  ASSERT_EQ(inject_messages.size(), 1u);
  EXPECT_TRUE(inject_messages.back().inject_chrome_input().is_active());
  EXPECT_EQ(user_data->GetSelectedTabs().size(), 1u);

  // Disassociating Tab 10 from the task removes its context and unmounts the
  // context library button (`is_active = false`).
  user_data->OnTaskDisassociatedFromTab(task_id, tab_id);
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  ASSERT_EQ(inject_messages.size(), 2u);
  EXPECT_FALSE(inject_messages.back().inject_chrome_input().is_active());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       AppendTabContextsToOnSubmitQueryResponse_RecordsZeroAttachedCount) {
  base::HistogramTester histogram_tester;

  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  auto mock_handle =
      std::make_unique<contextual_search::MockContextualSearchSessionHandle>();
  auto* session_handle_ptr = mock_handle.get();

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents_);
  helper->SetTaskSession(std::nullopt, std::move(mock_handle), nullptr);

  lens::ClientToSearchMessage response_message;
  user_data->AppendTabContextsToOnSubmitQueryResponseForTesting(
      &response_message, session_handle_ptr, std::nullopt);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission.Explicit", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission.Implicit", 0, 1);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       AppendTabContextsToOnSubmitQueryResponse_RecordsAttachedCount) {
  base::HistogramTester histogram_tester;

  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  auto mock_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* session_handle_ptr = mock_handle.get();

  auto token1 = base::UnguessableToken::Create();
  auto token2 = base::UnguessableToken::Create();
  auto token3 = base::UnguessableToken::Create();
  auto token_missing_req = base::UnguessableToken::Create();
  session_handle_ptr->GetUploadedContextTokensForTesting().push_back(token1);
  session_handle_ptr->GetUploadedContextTokensForTesting().push_back(token2);
  session_handle_ptr->GetUploadedContextTokensForTesting().push_back(token3);
  session_handle_ptr->GetUploadedContextTokensForTesting().push_back(
      token_missing_req);
  session_handle_ptr->AttachTabPendingUpload(
      token1, 1, GURL("https://example.com/1"), "Tab 1");
  session_handle_ptr->AttachTabPendingUpload(
      token2, 2, GURL("https://example.com/2"), "Tab 2");
  session_handle_ptr->AttachTabPendingUpload(
      token3, 3, GURL("https://example.com/3"), "Tab 3");
  session_handle_ptr->AttachTabPendingUpload(
      token_missing_req, 4, GURL("https://example.com/4"), "Tab 4");

  contextual_search::FileInfo file1;
  file1.file_token = token1;
  lens::LensOverlayRequestId req1;
  req1.set_context_id(101);
  file1.request_id = req1;
  file1.input_data = std::make_unique<lens::ContextualInputData>();
  file1.input_data->upload_type = lens::LensOverlayContextualInputUploadType::
      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;

  contextual_search::FileInfo file2;
  file2.file_token = token2;
  lens::LensOverlayRequestId req2;
  req2.set_context_id(202);
  file2.request_id = req2;

  contextual_search::FileInfo file3_implicit;
  file3_implicit.file_token = token3;
  file3_implicit.is_implicit_upload = true;
  lens::LensOverlayRequestId req3;
  req3.set_context_id(303);
  file3_implicit.request_id = req3;
  file3_implicit.input_data = std::make_unique<lens::ContextualInputData>();
  file3_implicit.input_data->is_implicit_upload = true;
  file3_implicit.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_RECONTEXTUALIZATION;

  // `file4_no_req` has no `request_id` and must not be added to the submission
  // proto or counted in the submission histograms.
  contextual_search::FileInfo file4_no_req;
  file4_no_req.file_token = token_missing_req;

  std::vector<contextual_search::FileInfo> uploaded_files = {
      file1, file2, file3_implicit, file4_no_req};
  ON_CALL(*session_handle_ptr, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents_);
  helper->SetTaskSession(std::nullopt, std::move(mock_handle), nullptr);

  lens::ClientToSearchMessage response_message;
  user_data->AppendTabContextsToOnSubmitQueryResponseForTesting(
      &response_message, session_handle_ptr, std::nullopt);

  EXPECT_EQ(response_message.on_submit_query_response().added_contexts_size(),
            3);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission.Explicit", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission.Implicit", 1, 1);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       TabContextSubscriptionUpdatesContextLibraryInputState) {
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  auto mock_handle =
      std::make_unique<contextual_search::MockContextualSearchSessionHandle>();
  auto* session_handle = mock_handle.get();
  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents_);
  helper->SetTaskSession(std::nullopt, std::move(mock_handle),
                         /*input_state_model=*/nullptr);

  std::vector<lens::ClientToSearchMessage> dispatched_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindRepeating(
      [](std::vector<lens::ClientToSearchMessage>* messages,
         const lens::ClientToSearchMessage& msg) { messages->push_back(msg); },
      &dispatched_messages));
  user_data->OnHandshakeComplete();
  dispatched_messages.clear();

  // Adding tab context directly to the session handle should notify user_data
  // and dispatch InjectChromeInput(CONTEXT_LIBRARY, true).
  base::UnguessableToken token = base::UnguessableToken::Create();
  session_handle->GetUploadedContextTokensForTesting().push_back(token);
  session_handle->AttachTabPendingUpload(
      token, /*tab_id=*/1, GURL("https://example.com"), "Example");

  ASSERT_EQ(dispatched_messages.size(), 2u);
  ASSERT_TRUE(dispatched_messages[0].has_inject_chrome_input());
  EXPECT_EQ(dispatched_messages[0].inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
  EXPECT_TRUE(dispatched_messages[0].inject_chrome_input().is_active());
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);

  // Removing tab context from the session handle should dispatch
  // InjectChromeInput(CONTEXT_LIBRARY, false).
  dispatched_messages.clear();
  session_handle->RemoveUploadedContextToken(token);

  ASSERT_EQ(dispatched_messages.size(), 2u);
  ASSERT_TRUE(dispatched_messages[0].has_inject_chrome_input());
  EXPECT_EQ(dispatched_messages[0].inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
  EXPECT_FALSE(dispatched_messages[0].inject_chrome_input().is_active());
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_NONE);

  // Replacing the session with a new session that already has attached tab
  // context should immediately update the context library input state via
  // OnSessionChanged().
  dispatched_messages.clear();
  auto mock_handle2 =
      std::make_unique<contextual_search::MockContextualSearchSessionHandle>();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  mock_handle2->GetUploadedContextTokensForTesting().push_back(token2);
  mock_handle2->AttachTabPendingUpload(
      token2, /*tab_id=*/2, GURL("https://example.com/2"), "Example 2");
  helper->SetTaskSession(std::nullopt, std::move(mock_handle2),
                         /*input_state_model=*/nullptr);

  ASSERT_EQ(dispatched_messages.size(), 2u);
  ASSERT_TRUE(dispatched_messages[0].has_inject_chrome_input());
  EXPECT_EQ(dispatched_messages[0].inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
  EXPECT_TRUE(dispatched_messages[0].inject_chrome_input().is_active());
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);

  // Taking the session handle should notify OnSessionChanged(nullptr) and
  // clear the context library input state.
  dispatched_messages.clear();
  auto taken_handle = helper->TakeSessionHandle();
  ASSERT_TRUE(taken_handle);

  ASSERT_EQ(dispatched_messages.size(), 2u);
  ASSERT_TRUE(dispatched_messages[0].has_inject_chrome_input());
  EXPECT_EQ(dispatched_messages[0].inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
  EXPECT_FALSE(dispatched_messages[0].inject_chrome_input().is_active());
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_NONE);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleThreadContextLibraryUpdateFromAim_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kContextualTasksContextLibrary);
  base::HistogramTester histogram_tester;

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  lens::SearchToClientMessage::UpdateThreadContextLibrary update;
  auto* ctx = update.add_contexts();
  ctx->set_context_id(1);
  ctx->mutable_webpage()->set_url("https://example.com");

  user_data->HandleThreadContextLibraryUpdateFromAim(update);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
      ContextualTasksWebContentsUserData::ThreadContextLibraryUpdateOutcome::
          kFeatureDisabled,
      1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.CommittedContextCount", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.CommittedTabContextCount", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.RestoredTabCount", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.TimeFromHandshakeToFirstUpdate", 0);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleThreadContextLibraryUpdateFromAim_NoSessionHandle) {
  base::test::ScopedFeatureList feature_list{kContextualTasksContextLibrary};
  base::HistogramTester histogram_tester;

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  // Setting a task_id without registering a session in
  // ContextualSearchWebContentsHelper causes GetOrCreateContextualSessionHandle
  // to return nullptr.
  user_data->SetTaskId(base::Uuid::GenerateRandomV4());

  lens::SearchToClientMessage::UpdateThreadContextLibrary update;
  auto* ctx = update.add_contexts();
  ctx->set_context_id(1);
  ctx->mutable_webpage()->set_url("https://example.com");

  user_data->HandleThreadContextLibraryUpdateFromAim(update);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
      ContextualTasksWebContentsUserData::ThreadContextLibraryUpdateOutcome::
          kNoSessionHandle,
      1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.CommittedContextCount", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.RestoredTabCount", 0);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleThreadContextLibraryUpdateFromAim_EmptyConvertedContext) {
  base::test::ScopedFeatureList feature_list{kContextualTasksContextLibrary};
  base::HistogramTester histogram_tester;

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  lens::SearchToClientMessage::UpdateThreadContextLibrary empty_update;
  user_data->HandleThreadContextLibraryUpdateFromAim(empty_update);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
      ContextualTasksWebContentsUserData::ThreadContextLibraryUpdateOutcome::
          kEmptyConvertedContext,
      1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.CommittedContextCount", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.RestoredTabCount", 0);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleThreadContextLibraryUpdateFromAim_SuccessWithoutTaskIdAndLatency) {
  base::test::ScopedFeatureList feature_list{kContextualTasksContextLibrary};
  base::HistogramTester histogram_tester;

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  lens::SearchToClientMessage message;
  auto* update = message.mutable_update_thread_context_library();
  auto* tab_ctx = update->add_contexts();
  tab_ctx->set_context_id(101);
  tab_ctx->set_has_chrome_tab_data(true);
  tab_ctx->mutable_webpage()->set_url("https://example.com/tab");
  tab_ctx->mutable_webpage()->set_title("Example Tab");

  auto* pdf_ctx = update->add_contexts();
  pdf_ctx->set_context_id(102);
  pdf_ctx->set_has_chrome_tab_data(false);
  pdf_ctx->mutable_pdf()->set_url("https://example.com/doc.pdf");
  pdf_ctx->mutable_pdf()->set_title("Doc PDF");

  std::vector<uint8_t> bytes(message.ByteSizeLong());
  message.SerializeToArray(bytes.data(), bytes.size());

  // First update after handshake records latency and counts.
  EXPECT_TRUE(user_data->OnSearchMessageReceived(bytes));

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
      ContextualTasksWebContentsUserData::ThreadContextLibraryUpdateOutcome::
          kSuccessWithoutTaskId,
      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.CommittedContextCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.CommittedTabContextCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.RestoredTabCount", 0, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.TimeFromHandshakeToFirstUpdate", 1);

  // A second update on the same page state does not re-record the first-update
  // latency histogram, but still records outcome and counts.
  EXPECT_TRUE(user_data->OnSearchMessageReceived(bytes));
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.TimeFromHandshakeToFirstUpdate", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
      ContextualTasksWebContentsUserData::ThreadContextLibraryUpdateOutcome::
          kSuccessWithoutTaskId,
      2);

  // Receiving OnPageChanged while connected resets the first-update timer so
  // the next thread update records latency again.
  lens::SearchToClientMessage page_changed_msg;
  page_changed_msg.mutable_on_page_changed()->set_is_zero_state(false);
  std::vector<uint8_t> page_changed_bytes(page_changed_msg.ByteSizeLong());
  page_changed_msg.SerializeToArray(page_changed_bytes.data(),
                                    page_changed_bytes.size());
  EXPECT_TRUE(user_data->OnSearchMessageReceived(page_changed_bytes));

  EXPECT_TRUE(user_data->OnSearchMessageReceived(bytes));
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ThreadContextLibrary.TimeFromHandshakeToFirstUpdate", 2);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(
    ContextualTasksWebContentsUserDataTest,
    HandleThreadContextLibraryUpdateFromAim_SuccessWithTaskIdAndRestoredTabs) {
  base::test::ScopedFeatureList feature_list{kContextualTasksContextLibrary};
  base::HistogramTester histogram_tester;

  ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext*) {
        return std::unique_ptr<KeyedService>(
            std::make_unique<testing::NiceMock<MockContextualTasksService>>());
      }));
  auto* mock_tasks_service = static_cast<MockContextualTasksService*>(
      ContextualTasksServiceFactory::GetForProfile(profile_.get()));
  ASSERT_NE(mock_tasks_service, nullptr);

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(task_id, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  // Set up a mock BrowserWindowInterface with an open tab matching one of the
  // incoming tab contexts.
  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host, mock_tab_list);

  content::WebContents* open_tab_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(open_tab_contents,
                                                   base::NullCallback());
  content::WebContentsTester::For(open_tab_contents)
      ->NavigateAndCommit(GURL("https://example.com/open-tab"));

  tabs::MockTabInterface mock_open_tab;
  ON_CALL(mock_open_tab, GetContents())
      .WillByDefault(testing::Return(open_tab_contents));
  std::vector<tabs::TabInterface*> all_tabs = {&mock_open_tab};
  ON_CALL(mock_tab_list, GetAllTabs()).WillByDefault(testing::Return(all_tabs));

  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->SetTaskId(task_id);

  EXPECT_CALL(*mock_tasks_service,
              SetUrlResourcesFromServer(task_id, testing::_))
      .Times(1);

  lens::SearchToClientMessage::UpdateThreadContextLibrary update;
  // Tab 1: matches `open_tab_contents`.
  auto* open_tab_ctx = update.add_contexts();
  open_tab_ctx->set_context_id(201);
  open_tab_ctx->set_has_chrome_tab_data(true);
  open_tab_ctx->mutable_webpage()->set_url("https://example.com/open-tab");
  open_tab_ctx->mutable_webpage()->set_title("Open Tab");

  // Tab 2: closed tab (not in `all_tabs`).
  auto* closed_tab_ctx = update.add_contexts();
  closed_tab_ctx->set_context_id(202);
  closed_tab_ctx->set_has_chrome_tab_data(true);
  closed_tab_ctx->mutable_webpage()->set_url("https://example.com/closed-tab");
  closed_tab_ctx->mutable_webpage()->set_title("Closed Tab");

  user_data->HandleThreadContextLibraryUpdateFromAim(update);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
      ContextualTasksWebContentsUserData::ThreadContextLibraryUpdateOutcome::
          kSuccessWithTaskId,
      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.CommittedContextCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.CommittedTabContextCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.RestoredTabCount", 1, 1);
  EXPECT_EQ(mock_handle->GetTabContextState().restored.size(), 1u);

  // Destroy WebContents (and ContextualTasksWebContentsUserData, which observes
  // `mock_tab_list` and `mock_tasks_service`) before stack-allocated mocks are
  // torn down.
  webui::SetBrowserWindowInterface(web_contents_, nullptr);
  web_contents_ = nullptr;
  web_contents_factory_.reset();
}
#endif

}  // namespace contextual_tasks
