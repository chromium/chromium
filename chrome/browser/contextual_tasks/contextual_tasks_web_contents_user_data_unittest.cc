// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#endif

namespace contextual_tasks {

#if !BUILDFLAG(IS_ANDROID)
class MockActiveTaskContextProvider : public ActiveTaskContextProvider {
 public:
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RefreshContext, (), (override));
  MOCK_METHOD(void,
              SetContextualTasksPanelController,
              (ContextualTasksPanelController*),
              (override));
  MOCK_METHOD(void, AddLocalTabUnderline, (tabs::TabHandle), (override));
  MOCK_METHOD(void, RemoveLocalTabUnderline, (tabs::TabHandle), (override));
  MOCK_METHOD(void, ClearAllLocalTabUnderlines, (), (override));
};
#endif

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
  base::test::ScopedFeatureList default_feature_list_{
      kContextualTasksForceEntryPointEligibility};
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
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
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
  file1.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
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
  file2.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
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
  implicit_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;

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
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
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
  ON_CALL(*session_handle_ptr, search_session_id())
      .WillByDefault(testing::Return("mock_session_id"));

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
  file1.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file1.input_data = std::make_unique<lens::ContextualInputData>();
  file1.input_data->upload_type = lens::LensOverlayContextualInputUploadType::
      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;

  contextual_search::FileInfo file2;
  file2.file_token = token2;
  lens::LensOverlayRequestId req2;
  req2.set_context_id(202);
  file2.request_id = req2;
  file2.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;

  contextual_search::FileInfo file3_implicit;
  file3_implicit.file_token = token3;
  file3_implicit.is_implicit_upload = true;
  lens::LensOverlayRequestId req3;
  req3.set_context_id(303);
  req3.set_media_type(lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE);
  file3_implicit.request_id = req3;
  file3_implicit.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  lens::LensOverlayRequestId viewport_req3;
  viewport_req3.set_uuid(304);
  viewport_req3.set_context_id(303);
  file3_implicit.viewport_request_id = viewport_req3;
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

  const auto& response = response_message.on_submit_query_response();
  ASSERT_EQ(response.added_contexts_size(), 4);
  EXPECT_EQ(response.added_contexts(2).request_id().context_id(), 303);
  EXPECT_EQ(response.added_contexts(2).request_id().media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE);
  EXPECT_EQ(response.added_contexts(3).search_session_id(), "mock_session_id");
  EXPECT_EQ(response.added_contexts(3).request_id().uuid(), 304u);
  EXPECT_EQ(response.added_contexts(3).request_id().context_id(), 303);
  EXPECT_EQ(response.added_contexts(3).request_id().media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  EXPECT_EQ(response.added_contexts(3).contextual_input_upload_type(),
            lens::LensOverlayContextualInputUploadType::
                CONTEXTUAL_INPUT_UPLOAD_TYPE_RECONTEXTUALIZATION);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission.Explicit", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission.Implicit", 1, 1);
}

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    HandleOnSubmitQueryRequest_WithoutRecentInteraction_PostsEmptyResponseAndPreservesContext) {
  auto mock_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* session_handle_ptr = mock_handle.get();
  ON_CALL(*session_handle_ptr, search_session_id())
      .WillByDefault(testing::Return("mock_session_id"));

  auto token1 = base::UnguessableToken::Create();
  session_handle_ptr->GetUploadedContextTokensForTesting().push_back(token1);
  session_handle_ptr->AttachTabPendingUpload(
      token1, 1, GURL("https://example.com/1"), "Tab 1");

  contextual_search::FileInfo file1;
  file1.file_token = token1;
  lens::LensOverlayRequestId req1;
  req1.set_context_id(101);
  file1.request_id = req1;
  file1.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  std::vector<contextual_search::FileInfo> uploaded_files = {file1};
  ON_CALL(*session_handle_ptr, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(mock_handle), nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  ASSERT_FALSE(web_contents_->HasRecentInteraction());

  user_data->HandleOnSubmitQueryRequest();

  ASSERT_EQ(submit_responses.size(), 1u);
  EXPECT_EQ(
      submit_responses[0].on_submit_query_response().added_contexts_size(), 0);
  EXPECT_EQ(
      submit_responses[0].on_submit_query_response().removed_contexts_size(),
      0);
  EXPECT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_EQ(session_handle_ptr->GetUploadedContextTokens().size(), 1u);
  EXPECT_EQ(session_handle_ptr->GetSubmittedContextTokens().size(), 0u);
}

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    HandleOnSubmitQueryRequest_ReplayInteraction_PostsEmptyResponseAndPreservesContext) {
  auto mock_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* session_handle_ptr = mock_handle.get();
  ON_CALL(*session_handle_ptr, search_session_id())
      .WillByDefault(testing::Return("mock_session_id"));

  auto token1 = base::UnguessableToken::Create();
  session_handle_ptr->GetUploadedContextTokensForTesting().push_back(token1);
  session_handle_ptr->AttachTabPendingUpload(
      token1, 1, GURL("https://example.com/1"), "Tab 1");

  contextual_search::FileInfo file1;
  file1.file_token = token1;
  lens::LensOverlayRequestId req1;
  req1.set_context_id(101);
  file1.request_id = req1;
  file1.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  std::vector<contextual_search::FileInfo> uploaded_files = {file1};
  ON_CALL(*session_handle_ptr, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(mock_handle), nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  // 1. First submission with user interaction attaches context.
  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest();

  ASSERT_EQ(submit_responses.size(), 1u);
  ASSERT_EQ(
      submit_responses[0].on_submit_query_response().added_contexts_size(), 1);
  EXPECT_EQ(submit_responses[0]
                .on_submit_query_response()
                .added_contexts(0)
                .request_id()
                .context_id(),
            101);
  EXPECT_EQ(session_handle_ptr->GetUploadedContextTokens().size(), 0u);
  EXPECT_EQ(session_handle_ptr->GetSubmittedContextTokens().size(), 1u);

  // 2. Attach a second tab and replay `OnSubmitQueryRequest` without a new
  // interaction: posts an empty response and preserves the pending tab context.
  submit_responses.clear();
  auto token2 = base::UnguessableToken::Create();
  session_handle_ptr->GetUploadedContextTokensForTesting().push_back(token2);
  session_handle_ptr->AttachTabPendingUpload(
      token2, 2, GURL("https://example.com/2"), "Tab 2");
  contextual_search::FileInfo file2;
  file2.file_token = token2;
  lens::LensOverlayRequestId req2;
  req2.set_context_id(202);
  file2.request_id = req2;
  file2.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  uploaded_files = {file2};

  user_data->HandleOnSubmitQueryRequest();

  ASSERT_EQ(submit_responses.size(), 1u);
  EXPECT_EQ(
      submit_responses[0].on_submit_query_response().added_contexts_size(), 0);
  EXPECT_EQ(
      submit_responses[0].on_submit_query_response().removed_contexts_size(),
      0);
  EXPECT_EQ(session_handle_ptr->GetUploadedContextTokens().size(), 1u);
  EXPECT_EQ(session_handle_ptr->GetSubmittedContextTokens().size(), 1u);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleOnSubmitQueryRequest_WhileSubmitPending_IgnoresDuplicateRequest) {
  auto mock_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* session_handle_ptr = mock_handle.get();
  ON_CALL(*session_handle_ptr, search_session_id())
      .WillByDefault(testing::Return("mock_session_id"));

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  session_handle_ptr->GetUploadedContextTokensForTesting().push_back(token1);
  session_handle_ptr->AttachTabPendingUpload(
      token1, 1, GURL("https://example.com/1"), "Tab 1");

  contextual_search::FileInfo file1;
  file1.file_token = token1;
  lens::LensOverlayRequestId req1;
  req1.set_context_id(101);
  file1.request_id = req1;
  file1.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  std::vector<contextual_search::FileInfo> uploaded_files = {file1};
  ON_CALL(*session_handle_ptr, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(task_id, std::move(mock_handle), nullptr);

  testing::NiceMock<MockContextualTasksService> mock_tasks_service;
  base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
      pending_context_callback;
  EXPECT_CALL(mock_tasks_service,
              GetContextForTask(task_id, testing::_, testing::_, testing::_))
      .WillOnce(
          [&pending_context_callback](
              const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              std::unique_ptr<ContextDecorationParams>,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) {
            pending_context_callback = std::move(callback);
          });

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->SetTaskId(task_id);
  user_data->SetContextualTasksServiceForTesting(&mock_tasks_service);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  // 1. Start the first submission; `GetContextForTask` holds completion so
  // `submit_pending()` remains true.
  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest();
  EXPECT_TRUE(user_data->submit_pending());
  ASSERT_TRUE(pending_context_callback);
  EXPECT_TRUE(submit_responses.empty());

  // 2. A duplicate `OnSubmitQueryRequest` arriving while `submit_pending()` is
  // true returns early without posting a premature empty response.
  user_data->HandleOnSubmitQueryRequest();
  EXPECT_TRUE(user_data->submit_pending());
  EXPECT_TRUE(submit_responses.empty());

  // 3. Completing the in-flight contextualization dispatches the single
  // contextualized response.
  std::move(pending_context_callback)
      .Run(std::make_unique<ContextualTaskContext>(ContextualTask(task_id)));
  EXPECT_FALSE(user_data->submit_pending());
  ASSERT_EQ(submit_responses.size(), 1u);
  ASSERT_EQ(
      submit_responses[0].on_submit_query_response().added_contexts_size(), 1);
  EXPECT_EQ(submit_responses[0]
                .on_submit_query_response()
                .added_contexts(0)
                .request_id()
                .context_id(),
            101);

  user_data->SetContextualTasksServiceForTesting(nullptr);
}

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    HandleOnSubmitQueryRequest_BeforeHandshakeComplete_DoesNotQueueEmptyResponse) {
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  // Before `OnHandshakeComplete()` is called, a rejected `OnSubmitQueryRequest`
  // should not queue an empty `OnSubmitQueryResponse` into
  // `pending_search_messages`.
  user_data->HandleOnSubmitQueryRequest();
  EXPECT_TRUE(submit_responses.empty());

  user_data->OnHandshakeComplete();
  EXPECT_TRUE(submit_responses.empty());
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
  EXPECT_THAT(dispatched_messages[1].on_context_state_changed().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_BROWSER_TAB));

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
  EXPECT_THAT(dispatched_messages[1].on_context_state_changed().input_types(),
              testing::IsEmpty());

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
  EXPECT_THAT(dispatched_messages[1].on_context_state_changed().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_BROWSER_TAB));

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
  EXPECT_THAT(dispatched_messages[1].on_context_state_changed().input_types(),
              testing::IsEmpty());
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
       ContextLibraryAndThreadUpdate_BlockedWhenCanShareTabContextFalse) {
  base::test::ScopedFeatureList feature_list{kContextualTasksContextLibrary};
  base::HistogramTester histogram_tester;

  // Disable tab context sharing via pref so `CanShareTabContext(profile_)` is
  // false.
  profile_->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  std::vector<lens::ClientToSearchMessage> dispatched_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindRepeating(
      [](std::vector<lens::ClientToSearchMessage>* messages,
         const lens::ClientToSearchMessage& msg) { messages->push_back(msg); },
      &dispatched_messages));
  user_data->OnHandshakeComplete();
  dispatched_messages.clear();

  // Attaching a tab to the session handle should neither mount
  // `CONTEXT_LIBRARY` nor emit `OnContextStateChanged` when
  // `CanShareTabContext()` is false.
  base::UnguessableToken token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting().push_back(token);
  mock_handle->AttachTabPendingUpload(token, /*tab_id=*/1,
                                      GURL("https://example.com"), "Example");
  EXPECT_TRUE(dispatched_messages.empty());

  // `HandleThreadContextLibraryUpdateFromAim()` should return early and record
  // `kFeatureDisabled` without populating restored tabs.
  lens::SearchToClientMessage::UpdateThreadContextLibrary update;
  auto* ctx = update.add_contexts();
  ctx->set_context_id(1);
  ctx->set_has_chrome_tab_data(true);
  ctx->mutable_webpage()->set_url("https://example.com");
  ctx->mutable_webpage()->set_title("Example");

  user_data->HandleThreadContextLibraryUpdateFromAim(update);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
      ContextualTasksWebContentsUserData::ThreadContextLibraryUpdateOutcome::
          kFeatureDisabled,
      1);
  EXPECT_TRUE(mock_handle->GetTabContextState().restored.empty());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandshakeResponseWithoutTabsCapability_DisablesTabFeatures) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksContextLibrary, {}},
       {kContextualTasksTabBasedSuggestions, {}},
       {kContextualTasksContext,
        {{kContextualTasksContextSmartTabSharing.name, "true"},
         {kContextualTasksContextSmartTabSharingRearchitecture.name, "true"}}},
       {kContextualTasksContextSmartTabSharingDefaultOnAvailability, {}}},
      {});
  profile_->GetPrefs()->SetBoolean(kContextualTasksShareOpenTabsEveryThread,
                                   true);
  base::HistogramTester histogram_tester;

  std::vector<contextual_search::FileInfo> uploaded_files;
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("suggest_session_123"));
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);
  base::ScopedClosureRunner clear_session(base::BindOnce(
      [](content::WebContents* wc) {
        ContextualSearchWebContentsHelper::GetOrCreateForWebContents(wc)
            ->SetTaskSession(std::nullopt, /*handle=*/nullptr,
                             /*input_state_model=*/nullptr);
      },
      web_contents_.get()));

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

#if !BUILDFLAG(IS_ANDROID)
  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockActiveTaskContextProvider>
      mock_active_task_context_provider;
  ui::ScopedUnownedUserData<ActiveTaskContextProvider>
      active_task_context_provider_registration(
          unowned_user_data_host, mock_active_task_context_provider);
  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);
  base::ScopedClosureRunner clear_browser_window(base::BindOnce(
      [](content::WebContents* wc) {
        webui::SetBrowserWindowInterface(wc, nullptr);
      },
      web_contents_.get()));
#endif

  // Before `HandshakeResponse` disables `TABS`, Smart Tab Sharing is active by
  // default when `CanShareTabContext(profile_)` is true.
  EXPECT_TRUE(user_data->IsSmartTabSharingEnabled());
  EXPECT_TRUE(user_data->IsSmartTabSharingActive());

  std::vector<lens::ClientToSearchMessage> dispatched_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindRepeating(
      [](std::vector<lens::ClientToSearchMessage>* messages,
         const lens::ClientToSearchMessage& msg) { messages->push_back(msg); },
      &dispatched_messages));
  base::ScopedClosureRunner reset_dispatcher(base::BindOnce(
      [](ContextualTasksWebContentsUserData* ud) {
        ud->SetSearchMessageDispatcherForTesting(base::NullCallback());
      },
      user_data));

  // Receive a `HandshakeResponse` without `LENS` (simulating a server before
  // the `capabilities` rollout). Tab sharing should remain active as normal.
  lens::SearchToClientMessage handshake_msg;
  auto* resp = handshake_msg.mutable_handshake_response();
  resp->set_auth_user_index(0);
  std::vector<uint8_t> handshake_bytes(handshake_msg.ByteSizeLong());
  handshake_msg.SerializeToArray(handshake_bytes.data(),
                                 handshake_bytes.size());
  EXPECT_TRUE(user_data->OnSearchMessageReceived(handshake_bytes));
  EXPECT_TRUE(user_data->IsSmartTabSharingActive());

  // Stage a ready manual tab while `TABS` is active so context state becomes
  // `CONTEXT_STATE_READY` and suggest inputs populate `added_contexts`.
  base::UnguessableToken ready_token = base::UnguessableToken::Create();
  contextual_search::FileInfo ready_file;
  ready_file.file_token = ready_token;
  lens::LensOverlayRequestId ready_req;
  ready_req.set_context_id(101);
  ready_file.request_id = ready_req;
  ready_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  uploaded_files.push_back(ready_file);
  mock_handle->GetUploadedContextTokensForTesting().push_back(ready_token);
  mock_handle->AttachTabPendingUpload(
      ready_token, /*tab_id=*/10, GURL("https://ready.example.com"), "Ready");

  // Receive a `HandshakeResponse` that advertises `LENS` only (omitting
  // `TABS`).
  dispatched_messages.clear();
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(mock_active_task_context_provider, RefreshContext()).Times(1);
#endif
  resp->add_capabilities(lens::SearchToClientMessage::HandshakeResponse::LENS);
  handshake_bytes.resize(handshake_msg.ByteSizeLong());
  handshake_msg.SerializeToArray(handshake_bytes.data(),
                                 handshake_bytes.size());
  EXPECT_TRUE(user_data->OnSearchMessageReceived(handshake_bytes));
#if !BUILDFLAG(IS_ANDROID)
  testing::Mock::VerifyAndClearExpectations(&mock_active_task_context_provider);
#endif

  // Smart Tab Sharing should now be inactive, `CONTEXT_LIBRARY` must be
  // unmounted, context state must reset to `CONTEXT_STATE_NONE`, and suggest
  // inputs must clear `added_contexts`.
  EXPECT_FALSE(user_data->IsSmartTabSharingActive());
  bool saw_context_library_unmount = false;
  bool saw_context_state_none = false;
  bool saw_suggest_inputs_cleared = false;
  for (const auto& msg : dispatched_messages) {
    if (msg.has_inject_chrome_input() &&
        msg.inject_chrome_input().input_type() ==
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY) {
      EXPECT_FALSE(msg.inject_chrome_input().is_active());
      saw_context_library_unmount = true;
    }
    if (msg.has_on_context_state_changed()) {
      EXPECT_EQ(msg.on_context_state_changed().context_state(),
                lens::ClientToSearchMessage::OnContextStateChanged::
                    CONTEXT_STATE_NONE);
      EXPECT_TRUE(msg.on_context_state_changed().input_types().empty());
      saw_context_state_none = true;
    }
    if (msg.has_on_suggest_inputs_changed()) {
      EXPECT_TRUE(msg.on_suggest_inputs_changed().added_contexts().empty());
      saw_suggest_inputs_cleared = true;
    }
  }
  EXPECT_TRUE(saw_context_library_unmount);
  EXPECT_TRUE(saw_context_state_none);
  EXPECT_TRUE(saw_suggest_inputs_cleared);

  // Attaching tab context to the session handle should not mount
  // `CONTEXT_LIBRARY`, emit `OnContextStateChanged`, or emit
  // `OnSuggestInputsChanged` while the page lacks the `TABS` capability.
  dispatched_messages.clear();
  base::UnguessableToken token = base::UnguessableToken::Create();
  contextual_search::FileInfo extra_file;
  extra_file.file_token = token;
  lens::LensOverlayRequestId extra_req;
  extra_req.set_context_id(102);
  extra_file.request_id = extra_req;
  extra_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  uploaded_files.push_back(extra_file);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token);
  mock_handle->AttachTabPendingUpload(token, /*tab_id=*/1,
                                      GURL("https://example.com"), "Example");
  EXPECT_TRUE(dispatched_messages.empty());

  // `HandleThreadContextLibraryUpdateFromAim()` should also be blocked when the
  // server handshake omitted `TABS`.
  lens::SearchToClientMessage::UpdateThreadContextLibrary update;
  auto* ctx = update.add_contexts();
  ctx->set_context_id(1);
  ctx->set_has_chrome_tab_data(true);
  ctx->mutable_webpage()->set_url("https://example.com");
  ctx->mutable_webpage()->set_title("Example");
  user_data->HandleThreadContextLibraryUpdateFromAim(update);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
      ContextualTasksWebContentsUserData::ThreadContextLibraryUpdateOutcome::
          kFeatureDisabled,
      1);
  EXPECT_TRUE(mock_handle->GetTabContextState().restored.empty());

  // If a subsequent `HandshakeResponse` includes `TABS`, Smart Tab Sharing,
  // `CONTEXT_LIBRARY` mounting, and suggest inputs become active again.
  dispatched_messages.clear();
  resp->add_capabilities(lens::SearchToClientMessage::HandshakeResponse::TABS);
  handshake_bytes.resize(handshake_msg.ByteSizeLong());
  handshake_msg.SerializeToArray(handshake_bytes.data(),
                                 handshake_bytes.size());
  EXPECT_TRUE(user_data->OnSearchMessageReceived(handshake_bytes));
  EXPECT_TRUE(user_data->IsSmartTabSharingActive());
  bool saw_context_library_mount = false;
  bool saw_suggest_inputs_restored = false;
  for (const auto& msg : dispatched_messages) {
    if (msg.has_inject_chrome_input() &&
        msg.inject_chrome_input().input_type() ==
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY &&
        msg.inject_chrome_input().is_active()) {
      saw_context_library_mount = true;
    }
    if (msg.has_on_suggest_inputs_changed()) {
      EXPECT_EQ(msg.on_suggest_inputs_changed().added_contexts_size(), 2);
      saw_suggest_inputs_restored = true;
    }
  }
  EXPECT_TRUE(saw_context_library_mount);
  EXPECT_TRUE(saw_suggest_inputs_restored);
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

  const SessionID open_tab_session_id =
      sessions::SessionTabHelper::IdForTab(open_tab_contents);
  EXPECT_CALL(*mock_tasks_service,
              SetUrlResourcesFromServer(task_id, testing::_))
      .WillOnce([open_tab_session_id](const base::Uuid&,
                                      std::vector<UrlResource> resources) {
        ASSERT_EQ(resources.size(), 2u);
        EXPECT_EQ(resources[0].tab_id, open_tab_session_id);
        EXPECT_FALSE(resources[1].tab_id.has_value());
      });

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

TEST_F(ContextualTasksWebContentsUserDataTest,
       SmartTabSharing_DisabledWhenRearchitectureParamFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksForceEntryPointEligibility, {}},
       {kContextualTasksContext,
        {{kContextualTasksContextSmartTabSharing.name, "true"}}}},
      {});

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  EXPECT_FALSE(user_data->IsSmartTabSharingEnabled());
  EXPECT_FALSE(user_data->IsSmartTabSharingActive());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       SmartTabSharing_ToggleMountsContextLibraryAndClearsManualTabs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksForceEntryPointEligibility, {}},
       {kContextualTasksContext,
        {{kContextualTasksContextSmartTabSharing.name, "true"},
         {kContextualTasksContextSmartTabSharingRearchitecture.name,
          "true"}}}},
      {});

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("sts_session_123"));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> sent_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&sent_messages](const lens::ClientToSearchMessage& message) {
        sent_messages.push_back(message);
      }));

  int tab_context_updated_notifications = 0;
  constexpr int kFrameId = 1;
  user_data->RegisterExtensionFrame(&kFrameId);
  user_data->UpdateExtensionFrameBound(
      &kFrameId, /*is_page_bound=*/true, base::DoNothing(),
      base::BindLambdaForTesting([&tab_context_updated_notifications]() {
        ++tab_context_updated_notifications;
      }));

  EXPECT_TRUE(user_data->IsSmartTabSharingEnabled());
  EXPECT_FALSE(user_data->IsSmartTabSharingActive());

  // Attach both a pending delayed manual tab and an uploaded manual tab first.
  mock_handle->AddPendingDelayedTab(10, GURL("https://example.com/manual"),
                                    "Manual Tab");
  base::UnguessableToken uploaded_manual_token =
      base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting().push_back(
      uploaded_manual_token);
  mock_handle->AttachTabPendingUpload(uploaded_manual_token, 20,
                                      GURL("https://example.com/manual2"),
                                      "Manual Tab 2");
  EXPECT_EQ(user_data->GetSelectedTabs().size(), 2u);

  // Toggling Smart Tab Sharing ON should clear manual unsubmitted tabs,
  // mount `CONTEXT_LIBRARY` (`is_active = true`), and notify bound extension
  // frames.
  sent_messages.clear();
  user_data->SetSmartTabSharingActive(true);
  EXPECT_TRUE(user_data->IsSmartTabSharingActive());
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  EXPECT_TRUE(mock_handle->GetTabContextState().attached.empty());
  EXPECT_GE(tab_context_updated_notifications, 1);

  // Even after clearing manual tabs, `CONTEXT_LIBRARY` remains active because
  // Smart Tab Sharing is active.
  bool saw_inactive_context_library = false;
  for (const auto& msg : sent_messages) {
    if (msg.has_inject_chrome_input() &&
        msg.inject_chrome_input().input_type() ==
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY &&
        !msg.inject_chrome_input().is_active()) {
      saw_inactive_context_library = true;
    }
  }
  EXPECT_FALSE(saw_inactive_context_library);

  // Toggling Smart Tab Sharing OFF with 0 manual tabs should unmount
  // `CONTEXT_LIBRARY` (`is_active = false`) and notify extension frames.
  sent_messages.clear();
  int notifications_before_off = tab_context_updated_notifications;
  user_data->SetSmartTabSharingActive(false);
  EXPECT_FALSE(user_data->IsSmartTabSharingActive());
  EXPECT_GT(tab_context_updated_notifications, notifications_before_off);

  bool saw_unmount = false;
  for (const auto& msg : sent_messages) {
    if (msg.has_inject_chrome_input() &&
        msg.inject_chrome_input().input_type() ==
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY &&
        !msg.inject_chrome_input().is_active()) {
      saw_unmount = true;
    }
  }
  EXPECT_TRUE(saw_unmount);

  // Toggling Smart Tab Sharing ON again with 0 manual tabs should mount
  // `CONTEXT_LIBRARY` (`is_active = true`).
  sent_messages.clear();
  user_data->SetSmartTabSharingActive(true);
  bool saw_mount = false;
  for (const auto& msg : sent_messages) {
    if (msg.has_inject_chrome_input() &&
        msg.inject_chrome_input().input_type() ==
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY &&
        msg.inject_chrome_input().is_active()) {
      saw_mount = true;
    }
  }
  EXPECT_TRUE(saw_mount);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       SmartTabSharing_ImplicitTabsExcludedFromGetSelectedTabs) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  // An implicit smart-shared tab has `is_implicit_upload = true` in its
  // `FileInfo`.
  base::UnguessableToken implicit_token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting().push_back(implicit_token);
  mock_handle->AttachTabPendingUpload(
      implicit_token, 10, GURL("https://example.com/implicit"), "Implicit Tab");
  contextual_search::FileInfo implicit_file;
  implicit_file.file_token = implicit_token;
  implicit_file.is_implicit_upload = true;

  // An explicit manual tab has `is_implicit_upload = false`.
  base::UnguessableToken manual_token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting().push_back(manual_token);
  mock_handle->AttachTabPendingUpload(
      manual_token, 20, GURL("https://example.com/manual"), "Manual Tab");
  contextual_search::FileInfo manual_file;
  manual_file.file_token = manual_token;
  manual_file.is_implicit_upload = false;

  std::vector<contextual_search::FileInfo> uploaded_files = {implicit_file,
                                                             manual_file};
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  // `GetSelectedTabs()` must only return the manual tab, excluding the
  // implicit smart-shared tab.
  std::vector<contextual_search::TabInfo> selected_tabs =
      user_data->GetSelectedTabs();
  ASSERT_EQ(selected_tabs.size(), 1u);
  EXPECT_EQ(selected_tabs[0].tab_id, 20);
  EXPECT_EQ(selected_tabs[0].context_token, manual_token);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       SmartTabSharing_ClearInputPlateStateResetsOverrideToDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kContextualTasksForceEntryPointEligibility, {}},
       {kContextualTasksContext,
        {{kContextualTasksContextSmartTabSharing.name, "true"},
         {kContextualTasksContextSmartTabSharingRearchitecture.name, "true"}}},
       {kContextualTasksContextSmartTabSharingDefaultOnAvailability, {}}},
      {});
  profile_->GetPrefs()->SetBoolean(kContextualTasksShareOpenTabsEveryThread,
                                   true);

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  // Default-on when enabled and no per-thread override exists.
  EXPECT_TRUE(user_data->IsSmartTabSharingEnabled());
  EXPECT_TRUE(user_data->IsSmartTabSharingActive());

  // User opts out mid-thread.
  user_data->SetSmartTabSharingActive(false);
  EXPECT_FALSE(user_data->IsSmartTabSharingActive());

  // Starting a new thread (`ClearInputPlateState`) resets the per-thread
  // override back to the default-on state.
  user_data->ClearInputPlateState();
  EXPECT_TRUE(user_data->IsSmartTabSharingActive());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       ConstDebugAccessorsDoNotMutateOrCreateSession) {
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  const auto* const_user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_NE(const_user_data, nullptr);

  ContextualTasksWebContentsUserData::PageSearchDebugState initial_debug_state =
      const_user_data->GetPageSearchDebugState();
  EXPECT_TRUE(initial_debug_state.connected_document_id.empty());
  EXPECT_FALSE(initial_debug_state.is_handshake_complete);
  EXPECT_EQ(initial_debug_state.pending_search_messages_count, 0u);
  EXPECT_TRUE(const_user_data->extension_frames().empty());
  EXPECT_TRUE(const_user_data->pending_context_uploads().empty());
  EXPECT_TRUE(const_user_data->failed_context_uploads().empty());
  EXPECT_FALSE(const_user_data->submit_pending());
  EXPECT_TRUE(const_user_data->GetSelectedTabsSnapshot().empty());
  EXPECT_EQ(const_user_data->GetExistingContextualSessionHandle(), nullptr);

  auto* mutable_user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  mutable_user_data->OnDocumentConnected(web_contents_->GetPrimaryMainFrame());
  mutable_user_data->OnHandshakeComplete();
  mutable_user_data->RecordTabIdMapping(/*tab_handle_id=*/42,
                                        /*session_tab_id=*/99);

  ContextualTasksWebContentsUserData::PageSearchDebugState updated_debug_state =
      const_user_data->GetPageSearchDebugState();
  EXPECT_FALSE(updated_debug_state.connected_document_id.empty());
  EXPECT_TRUE(updated_debug_state.is_handshake_complete);

  const base::flat_map<int32_t, int32_t>& mappings =
      const_user_data->tab_handle_to_session_id();
  ASSERT_EQ(mappings.size(), 1u);
  EXPECT_EQ(mappings.at(42), 99);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       AppendTabContextsToOnSubmitQueryResponse_ExcludesInvalidUploads) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* session_handle_ptr = session_handle.get();
  ON_CALL(*session_handle_ptr, search_session_id())
      .WillByDefault(testing::Return("mock_session_id"));

  auto token1 = base::UnguessableToken::Create();  // superseded
  auto token2 = base::UnguessableToken::Create();  // failed
  auto token3 = base::UnguessableToken::Create();  // expired
  auto token4 = base::UnguessableToken::Create();  // un-uploaded
  auto token5 = base::UnguessableToken::Create();  // valid

  session_handle_ptr->GetUploadedContextTokensForTesting() = {
      token1, token2, token3, token4, token5};

  session_handle_ptr->AttachTabPendingUpload(
      token1, 1, GURL("https://example.com/1"), "Tab 1");
  session_handle_ptr->AttachTabPendingUpload(
      token2, 2, GURL("https://example.com/2"), "Tab 2");
  session_handle_ptr->AttachTabPendingUpload(
      token3, 3, GURL("https://example.com/3"), "Tab 3");
  session_handle_ptr->AttachTabPendingUpload(
      token4, 4, GURL("https://example.com/4"), "Tab 4");
  session_handle_ptr->AttachTabPendingUpload(
      token5, 5, GURL("https://example.com/5"), "Tab 5");

  contextual_search::FileInfo file1;
  file1.file_token = token1;
  lens::LensOverlayRequestId req1;
  req1.set_context_id(101);
  file1.request_id = req1;
  file1.is_superceded = true;
  file1.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file1.input_data = std::make_unique<lens::ContextualInputData>();
  file1.input_data->upload_type = lens::LensOverlayContextualInputUploadType::
      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;

  contextual_search::FileInfo file2;
  file2.file_token = token2;
  lens::LensOverlayRequestId req2;
  req2.set_context_id(102);
  file2.request_id = req2;
  file2.upload_status = contextual_search::ContextUploadStatus::kUploadFailed;

  contextual_search::FileInfo file3;
  file3.file_token = token3;
  lens::LensOverlayRequestId req3;
  req3.set_context_id(103);
  file3.request_id = req3;
  file3.upload_status = contextual_search::ContextUploadStatus::kUploadExpired;

  contextual_search::FileInfo file4;
  file4.file_token = token4;
  lens::LensOverlayRequestId req4;
  req4.set_context_id(104);
  file4.request_id = req4;
  file4.upload_status = contextual_search::ContextUploadStatus::kNotUploaded;

  contextual_search::FileInfo file5;
  file5.file_token = token5;
  lens::LensOverlayRequestId req5;
  req5.set_context_id(105);
  file5.request_id = req5;
  file5.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file5.input_data = std::make_unique<lens::ContextualInputData>();
  file5.input_data->upload_type = lens::LensOverlayContextualInputUploadType::
      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;

  std::vector<contextual_search::FileInfo> uploaded_files = {
      file1, file2, file3, file4, file5};
  ON_CALL(*session_handle_ptr, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents_);
  helper->SetTaskSession(std::nullopt, std::move(session_handle), nullptr);

  lens::ClientToSearchMessage response_message;
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  user_data->AppendTabContextsToOnSubmitQueryResponseForTesting(
      &response_message, session_handle_ptr, std::nullopt);

  ASSERT_TRUE(response_message.has_on_submit_query_response());
  const auto& response = response_message.on_submit_query_response();
  ASSERT_EQ(response.added_contexts_size(), 1);
  EXPECT_EQ(response.added_contexts(0).request_id().context_id(), 105);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextualTasksWebContentsUserDataTest,
       UpdateTabPickerVisibility_InjectsAndHidesTabPicker) {
  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host, mock_tab_list);

  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  std::vector<lens::ClientToSearchMessage> tab_picker_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage& message) {
        if (message.has_inject_chrome_input() &&
            message.inject_chrome_input().input_type() ==
                lens::ClientToSearchMessage::InjectChromeInput::TAB_PICKER) {
          tab_picker_messages.push_back(message);
        }
      }));

  // Pre-handshake calls do not queue or dispatch `TAB_PICKER`, and handshake
  // with no eligible tabs does not inject `TAB_PICKER`.
  content::WebContents* other_tab_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  content::WebContentsTester::For(other_tab_contents)
      ->NavigateAndCommit(GURL("https://example.com/pre-handshake"));
  tabs::MockTabInterface mock_other_tab;
  ON_CALL(mock_other_tab, GetContents())
      .WillByDefault(testing::Return(other_tab_contents));
  std::vector<tabs::TabInterface*> all_tabs = {&mock_other_tab};
  ON_CALL(mock_tab_list, GetAllTabs())
      .WillByDefault(testing::ReturnPointee(&all_tabs));
  user_data->UpdateTabPickerVisibility();
  EXPECT_TRUE(tab_picker_messages.empty());

  all_tabs.clear();
  user_data->OnHandshakeComplete();
  EXPECT_TRUE(tab_picker_messages.empty());

  // An ineligible tab (`chrome://newtab`, an AIM search URL, or an AIM
  // zero-state URL) or the host WebContents itself does not inject
  // `TAB_PICKER`.
  content::WebContentsTester::For(other_tab_contents)
      ->NavigateAndCommit(GURL("chrome://newtab"));
  tabs::MockTabInterface mock_self_tab;
  ON_CALL(mock_self_tab, GetContents())
      .WillByDefault(testing::Return(web_contents_));
  all_tabs = {&mock_other_tab, &mock_self_tab};
  user_data->UpdateTabPickerVisibility();
  EXPECT_TRUE(tab_picker_messages.empty());

  content::WebContentsTester::For(other_tab_contents)
      ->NavigateAndCommit(GURL("https://www.google.com/search?udm=50&q=test"));
  user_data->UpdateTabPickerVisibility();
  EXPECT_TRUE(tab_picker_messages.empty());

  content::WebContentsTester::For(other_tab_contents)
      ->NavigateAndCommit(GURL("https://www.google.com/search?udm=50"));
  user_data->UpdateTabPickerVisibility();
  EXPECT_TRUE(tab_picker_messages.empty());

  // Navigating `other_tab_contents` to an eligible URL injects `TAB_PICKER`
  // (`is_active = true`), and redundant calls are deduplicated.
  content::WebContentsTester::For(other_tab_contents)
      ->NavigateAndCommit(GURL("https://example.com/page"));
  user_data->UpdateTabPickerVisibility();
  ASSERT_EQ(tab_picker_messages.size(), 1u);
  EXPECT_TRUE(tab_picker_messages.back().inject_chrome_input().is_active());

  user_data->UpdateTabPickerVisibility();
  EXPECT_EQ(tab_picker_messages.size(), 1u);

  // When no eligible tabs remain, `UpdateTabPickerVisibility()` sends
  // `INJECT_INPUT` (`is_active = false`) to hide the tab picker.
  all_tabs.clear();
  user_data->UpdateTabPickerVisibility();
  ASSERT_EQ(tab_picker_messages.size(), 2u);
  EXPECT_FALSE(tab_picker_messages.back().inject_chrome_input().is_active());

  user_data->UpdateTabPickerVisibility();
  EXPECT_EQ(tab_picker_messages.size(), 2u);

  // Reconnecting a document resets `tab_picker_is_active` and injects
  // `TAB_PICKER` on handshake when an eligible tab is already open.
  all_tabs = {&mock_other_tab};
  tab_picker_messages.clear();
  user_data->OnDocumentConnected(web_contents_->GetPrimaryMainFrame());
  user_data->OnHandshakeComplete();
  ASSERT_EQ(tab_picker_messages.size(), 1u);
  EXPECT_TRUE(tab_picker_messages.back().inject_chrome_input().is_active());

  webui::SetBrowserWindowInterface(web_contents_, nullptr);
}

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    AppendTabContextsToOnSubmitQueryResponse_PopulatesActiveTabContextTurnMetadata) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("test_session"));

  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host, mock_tab_list);

  content::WebContents* active_tab_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(active_tab_contents,
                                                   base::NullCallback());
  const SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab_contents);

  content::WebContents* background_tab_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(background_tab_contents,
                                                   base::NullCallback());
  const SessionID background_tab_id =
      sessions::SessionTabHelper::IdForTab(background_tab_contents);

  tabs::MockTabInterface mock_active_tab;
  ON_CALL(mock_active_tab, GetContents())
      .WillByDefault(testing::Return(active_tab_contents));
  ON_CALL(mock_tab_list, GetActiveTab())
      .WillByDefault(testing::Return(&mock_active_tab));

  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  base::UnguessableToken active_token = base::UnguessableToken::Create();
  base::UnguessableToken background_token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting() = {active_token,
                                                       background_token};
  mock_handle->AttachTabPendingUpload(active_token, active_tab_id.id(),
                                      GURL("https://example.com/active"),
                                      "Active Tab");
  mock_handle->AttachTabPendingUpload(background_token, background_tab_id.id(),
                                      GURL("https://example.com/background"),
                                      "Background Tab");

  contextual_search::FileInfo active_file;
  active_file.file_token = active_token;
  active_file.tab_session_id = active_tab_id;
  active_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  lens::LensOverlayRequestId active_req;
  active_req.set_context_id(101);
  active_file.request_id = active_req;

  contextual_search::FileInfo background_file;
  background_file.file_token = background_token;
  background_file.tab_session_id = background_tab_id;
  background_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  lens::LensOverlayRequestId background_req;
  background_req.set_context_id(202);
  background_file.request_id = background_req;

  std::vector<contextual_search::FileInfo> uploaded_files = {active_file,
                                                             background_file};
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  lens::ClientToSearchMessage response_message;
  user_data->AppendTabContextsToOnSubmitQueryResponseForTesting(
      &response_message, mock_handle, std::nullopt);

  const auto& submit_response = response_message.on_submit_query_response();
  EXPECT_EQ(submit_response.added_contexts_size(), 2);
  ASSERT_EQ(submit_response.context_turn_metadata_size(), 1);
  EXPECT_EQ(submit_response.context_turn_metadata(0).context_id(), 101);
  EXPECT_TRUE(
      submit_response.context_turn_metadata(0).tab_metadata().is_active_tab());

  webui::SetBrowserWindowInterface(web_contents_, nullptr);
}

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    AppendTabContextsToOnSubmitQueryResponse_PopulatesActiveTabFromSubmittedContext) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("test_session"));

  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host, mock_tab_list);

  content::WebContents* active_tab_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(active_tab_contents,
                                                   base::NullCallback());
  const SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab_contents);

  tabs::MockTabInterface mock_active_tab;
  ON_CALL(mock_active_tab, GetContents())
      .WillByDefault(testing::Return(active_tab_contents));
  ON_CALL(mock_tab_list, GetActiveTab())
      .WillByDefault(testing::Return(&mock_active_tab));

  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  lens::LensOverlayRequestId submitted_req;
  submitted_req.set_context_id(303);
  mock_handle->SetPersistedTabs(
      {{active_tab_id, {base::UnguessableToken::Create(), submitted_req}}});

  lens::ClientToSearchMessage response_message;
  user_data->AppendTabContextsToOnSubmitQueryResponseForTesting(
      &response_message, mock_handle, std::nullopt);

  const auto& submit_response = response_message.on_submit_query_response();
  EXPECT_EQ(submit_response.added_contexts_size(), 0);
  ASSERT_EQ(submit_response.context_turn_metadata_size(), 1);
  EXPECT_EQ(submit_response.context_turn_metadata(0).context_id(), 303);
  EXPECT_TRUE(
      submit_response.context_turn_metadata(0).tab_metadata().is_active_tab());

  webui::SetBrowserWindowInterface(web_contents_, nullptr);
}

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    AppendTabContextsToOnSubmitQueryResponse_OmitsContextTurnMetadataWhenActiveTabNotAttached) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("test_session"));

  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host, mock_tab_list);

  content::WebContents* active_tab_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(active_tab_contents,
                                                   base::NullCallback());

  content::WebContents* background_tab_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(background_tab_contents,
                                                   base::NullCallback());
  const SessionID background_tab_id =
      sessions::SessionTabHelper::IdForTab(background_tab_contents);

  tabs::MockTabInterface mock_active_tab;
  ON_CALL(mock_active_tab, GetContents())
      .WillByDefault(testing::Return(active_tab_contents));
  ON_CALL(mock_tab_list, GetActiveTab())
      .WillByDefault(testing::Return(&mock_active_tab));

  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  base::UnguessableToken background_token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting() = {background_token};
  mock_handle->AttachTabPendingUpload(background_token, background_tab_id.id(),
                                      GURL("https://example.com/background"),
                                      "Background Tab");

  contextual_search::FileInfo background_file;
  background_file.file_token = background_token;
  background_file.tab_session_id = background_tab_id;
  background_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  lens::LensOverlayRequestId background_req;
  background_req.set_context_id(202);
  background_file.request_id = background_req;

  std::vector<contextual_search::FileInfo> uploaded_files = {background_file};
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  lens::ClientToSearchMessage response_message;
  user_data->AppendTabContextsToOnSubmitQueryResponseForTesting(
      &response_message, mock_handle, std::nullopt);

  const auto& submit_response = response_message.on_submit_query_response();
  EXPECT_EQ(submit_response.added_contexts_size(), 1);
  EXPECT_EQ(submit_response.context_turn_metadata_size(), 0);

  webui::SetBrowserWindowInterface(web_contents_, nullptr);
}
#endif

TEST_F(ContextualTasksWebContentsUserDataTest,
       UpdateContextState_PopulatesInputTypesAndDeduplicatesBySerializedBytes) {
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_NE(user_data, nullptr);

  auto mock_handle =
      std::make_unique<contextual_search::MockContextualSearchSessionHandle>();
  auto* session_handle = mock_handle.get();
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(mock_handle),
                       /*input_state_model=*/nullptr);

  std::vector<lens::ClientToSearchMessage::OnContextStateChanged>
      context_state_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&context_state_messages](const lens::ClientToSearchMessage& message) {
        if (message.has_on_context_state_changed()) {
          context_state_messages.push_back(message.on_context_state_changed());
        }
      }));

  // Initial handshake with no attached context should not emit a spurious
  // CONTEXT_STATE_NONE message.
  user_data->OnHandshakeComplete();
  EXPECT_TRUE(context_state_messages.empty());

  // Mounting a Lens region crop emits CONTEXT_STATE_READY with
  // INPUT_TYPE_LENS_IMAGE.
  user_data->OnLensThumbnailCreated("data:image/png;base64,crop_1");
  ASSERT_EQ(context_state_messages.size(), 1u);
  EXPECT_EQ(
      context_state_messages.back().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
  EXPECT_THAT(context_state_messages.back().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_LENS_IMAGE));

  // Replacing the Lens crop URI in-place keeps serialized OnContextStateChanged
  // bytes identical, so no duplicate message is emitted.
  context_state_messages.clear();
  user_data->OnLensThumbnailCreated("data:image/png;base64,crop_2");
  EXPECT_TRUE(context_state_messages.empty());

  // Attaching Tab 1 (uploading) emits CONTEXT_STATE_UPLOADING with both the
  // Lens crop and 1 browser tab.
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(token1);
  session_handle->GetUploadedContextTokensForTesting().push_back(token1);
  session_handle->AttachTabPendingUpload(
      token1, /*tab_id=*/1, GURL("https://example.com/1"), "Tab 1");
  ASSERT_EQ(context_state_messages.size(), 1u);
  EXPECT_EQ(context_state_messages.back().context_state(),
            lens::ClientToSearchMessage::OnContextStateChanged::
                CONTEXT_STATE_UPLOADING);
  EXPECT_THAT(context_state_messages.back().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_LENS_IMAGE,
                                   omnibox::INPUT_TYPE_BROWSER_TAB));

  // Attaching Tab 2 (also uploading) while ContextState is already
  // CONTEXT_STATE_UPLOADING changes `input_types`, so serialized-bytes
  // deduplication dispatches an updated message.
  context_state_messages.clear();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(token2);
  session_handle->GetUploadedContextTokensForTesting().push_back(token2);
  session_handle->AttachTabPendingUpload(
      token2, /*tab_id=*/2, GURL("https://example.com/2"), "Tab 2");
  ASSERT_EQ(context_state_messages.size(), 1u);
  EXPECT_EQ(context_state_messages.back().context_state(),
            lens::ClientToSearchMessage::OnContextStateChanged::
                CONTEXT_STATE_UPLOADING);
  EXPECT_THAT(context_state_messages.back().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_LENS_IMAGE,
                                   omnibox::INPUT_TYPE_BROWSER_TAB,
                                   omnibox::INPUT_TYPE_BROWSER_TAB));

  // Failing Tab 1's upload excludes it from `input_types` and emits an updated
  // message even though Tab 2 keeps ContextState at UPLOADING.
  context_state_messages.clear();
  user_data->OnContextUploadStatusChanged(
      token1, lens::MimeType::kAnnotatedPageContent,
      contextual_search::ContextUploadStatus::kValidationFailed, std::nullopt);
  ASSERT_EQ(context_state_messages.size(), 1u);
  EXPECT_EQ(context_state_messages.back().context_state(),
            lens::ClientToSearchMessage::OnContextStateChanged::
                CONTEXT_STATE_UPLOADING);
  EXPECT_THAT(context_state_messages.back().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_LENS_IMAGE,
                                   omnibox::INPUT_TYPE_BROWSER_TAB));

  // Redundant status notification with identical serialized bytes is
  // deduplicated.
  context_state_messages.clear();
  user_data->OnContextUploadStatusChanged(
      token1, lens::MimeType::kAnnotatedPageContent,
      contextual_search::ContextUploadStatus::kValidationFailed, std::nullopt);
  EXPECT_TRUE(context_state_messages.empty());

  // Completing Tab 2's upload transitions to CONTEXT_STATE_READY while keeping
  // `[LENS_IMAGE, BROWSER_TAB]`.
  user_data->OnContextUploadStatusChanged(
      token2, lens::MimeType::kAnnotatedPageContent,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  ASSERT_EQ(context_state_messages.size(), 1u);
  EXPECT_EQ(
      context_state_messages.back().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
  EXPECT_THAT(context_state_messages.back().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_LENS_IMAGE,
                                   omnibox::INPUT_TYPE_BROWSER_TAB));

  // Attaching Tab 3 (already ready) while ContextState is already
  // CONTEXT_STATE_READY emits a new message with 2 BROWSER_TAB entries.
  context_state_messages.clear();
  base::UnguessableToken token3 = base::UnguessableToken::Create();
  session_handle->GetUploadedContextTokensForTesting().push_back(token3);
  session_handle->AttachTabPendingUpload(
      token3, /*tab_id=*/3, GURL("https://example.com/3"), "Tab 3");
  ASSERT_EQ(context_state_messages.size(), 1u);
  EXPECT_EQ(
      context_state_messages.back().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
  EXPECT_THAT(context_state_messages.back().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_LENS_IMAGE,
                                   omnibox::INPUT_TYPE_BROWSER_TAB,
                                   omnibox::INPUT_TYPE_BROWSER_TAB));

  // Removing the Lens crop while remaining CONTEXT_STATE_READY drops
  // INPUT_TYPE_LENS_IMAGE and emits an updated message.
  context_state_messages.clear();
  user_data->RemoveLensCrop();
  ASSERT_EQ(context_state_messages.size(), 1u);
  EXPECT_EQ(
      context_state_messages.back().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
  EXPECT_THAT(context_state_messages.back().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_BROWSER_TAB,
                                   omnibox::INPUT_TYPE_BROWSER_TAB));

  // Reconnecting the document resets serialized deduplication state so
  // OnHandshakeComplete() re-sends the active context state.
  context_state_messages.clear();
  user_data->OnDocumentConnected(web_contents_->GetPrimaryMainFrame());
  user_data->OnHandshakeComplete();
  ASSERT_EQ(context_state_messages.size(), 1u);
  EXPECT_EQ(
      context_state_messages.back().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
  EXPECT_THAT(context_state_messages.back().input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_BROWSER_TAB,
                                   omnibox::INPUT_TYPE_BROWSER_TAB));

  // Removing `token1` (already failed) does not change the serialized message,
  // whereas removing `token2` and `token3` emits 2 messages (`[BROWSER_TAB]`
  // followed by `CONTEXT_STATE_NONE` with empty `input_types`).
  context_state_messages.clear();
  session_handle->RemoveUploadedContextToken(token1);
  session_handle->RemoveUploadedContextToken(token2);
  session_handle->RemoveUploadedContextToken(token3);
  ASSERT_EQ(context_state_messages.size(), 2u);
  EXPECT_EQ(
      context_state_messages[0].context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
  EXPECT_THAT(context_state_messages[0].input_types(),
              testing::ElementsAre(omnibox::INPUT_TYPE_BROWSER_TAB));
  EXPECT_EQ(
      context_state_messages[1].context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_NONE);
  EXPECT_THAT(context_state_messages[1].input_types(), testing::IsEmpty());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleOnSubmitQueryRequest_WaitsForInFlightManualTabUpload) {
  std::vector<contextual_search::FileInfo> uploaded_files;
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("manual_upload_session"));
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> sent_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&sent_messages](const lens::ClientToSearchMessage& message) {
        sent_messages.push_back(message);
      }));

  // 1. Attach a manual tab whose upload is still in flight.
  base::UnguessableToken token = base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(token);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token);
  mock_handle->AttachTabPendingUpload(token, 10, GURL("https://example.com/1"),
                                      "Tab 1");

  // 2. Submit a query while the manual tab upload is still in flight.
  sent_messages.clear();

  lens::SearchToClientMessage submit_msg;
  submit_msg.mutable_on_submit_query_request()->set_query_text("summarize tab");
  std::vector<uint8_t> submit_bytes(submit_msg.ByteSizeLong());
  submit_msg.SerializeToArray(submit_bytes.data(), submit_bytes.size());

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  EXPECT_TRUE(user_data->OnSearchMessageReceived(submit_bytes));
  EXPECT_TRUE(sent_messages.empty());
  EXPECT_TRUE(mock_handle->previous_turns().empty());

  // Non-terminal upload status updates (`kProcessing`, `kUploadStarted`) and a
  // duplicate submit request while deferred must not prematurely send
  // `OnSubmitQueryResponse`.
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kProcessing, std::nullopt);
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadStarted, std::nullopt);
  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("duplicate submit");
  EXPECT_TRUE(sent_messages.empty());
  EXPECT_TRUE(mock_handle->previous_turns().empty());

  // 3. When the manual tab upload finishes with `kUploadSuccessful`, the
  // deferred `OnSubmitQueryResponse` is sent with the uploaded tab's
  // `AddedContext` and the original query text is recorded in the thread turn.
  contextual_search::FileInfo file_info;
  file_info.file_token = token;
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(101);
  file_info.request_id = req_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info.input_data = std::make_unique<lens::ContextualInputData>();
  file_info.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;
  uploaded_files.push_back(std::move(file_info));

  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_EQ(mock_handle->previous_turns().size(), 1u);
  EXPECT_EQ(mock_handle->previous_turns()[0].query, "summarize tab");

  ASSERT_GE(sent_messages.size(), 2u);
  ASSERT_TRUE(sent_messages[0].has_on_submit_query_response());
  const auto& submit_response = sent_messages[0].on_submit_query_response();
  ASSERT_EQ(submit_response.added_contexts_size(), 1);
  EXPECT_EQ(submit_response.added_contexts(0).search_session_id(),
            "manual_upload_session");
  EXPECT_EQ(submit_response.added_contexts(0).request_id().context_id(), 101);
  EXPECT_EQ(submit_response.added_contexts(0).contextual_input_upload_type(),
            lens::LensOverlayContextualInputUploadType::
                CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);

  ASSERT_TRUE(sent_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      sent_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_NONE);

  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(
          testing::Return(std::vector<contextual_search::FileInfo>{}));
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleOnSubmitQueryRequest_WaitsForAllMultipleInFlightManualTabUploads) {
  std::vector<contextual_search::FileInfo> uploaded_files;
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("multi_manual_session"));
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(token1);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token1);
  mock_handle->AttachTabPendingUpload(token1, 10, GURL("https://example.com/1"),
                                      "Tab 1");
  user_data->OnTabContextUploadStarted(token2);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token2);
  mock_handle->AttachTabPendingUpload(token2, 20, GURL("https://example.com/2"),
                                      "Tab 2");

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("compare tabs");
  EXPECT_TRUE(submit_responses.empty());

  // Tab 1 finishes uploading first; Tab 2 is still in flight, so submission
  // remains deferred.
  contextual_search::FileInfo file1;
  file1.file_token = token1;
  lens::LensOverlayRequestId req1;
  req1.set_context_id(101);
  file1.request_id = req1;
  file1.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  uploaded_files.push_back(file1);

  user_data->OnContextUploadStatusChanged(
      token1, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  EXPECT_TRUE(submit_responses.empty());

  // Tab 2 finishes uploading -> `OnSubmitQueryResponse` is sent with both tabs.
  contextual_search::FileInfo file2;
  file2.file_token = token2;
  lens::LensOverlayRequestId req2;
  req2.set_context_id(202);
  file2.request_id = req2;
  file2.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  uploaded_files.push_back(file2);

  user_data->OnContextUploadStatusChanged(
      token2, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_EQ(submit_responses.size(), 1u);
  const auto& resp = submit_responses[0].on_submit_query_response();
  ASSERT_EQ(resp.added_contexts_size(), 2);
  EXPECT_EQ(resp.added_contexts(0).request_id().context_id(), 101);
  EXPECT_EQ(resp.added_contexts(1).request_id().context_id(), 202);

  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(
          testing::Return(std::vector<contextual_search::FileInfo>{}));
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleOnSubmitQueryRequest_UnblocksOnTerminalUploadFailureStatuses) {
  std::vector<contextual_search::FileInfo> uploaded_files;
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("failure_session"));
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  const contextual_search::ContextUploadStatus kFailureStatuses[] = {
      contextual_search::ContextUploadStatus::kUploadFailed,
      contextual_search::ContextUploadStatus::kValidationFailed,
      contextual_search::ContextUploadStatus::kUploadExpired,
      contextual_search::ContextUploadStatus::kUploadReplaced,
  };

  for (auto failure_status : kFailureStatuses) {
    submit_responses.clear();

    base::UnguessableToken ready_token = base::UnguessableToken::Create();
    base::UnguessableToken failing_token = base::UnguessableToken::Create();

    contextual_search::FileInfo ready_file;
    ready_file.file_token = ready_token;
    lens::LensOverlayRequestId ready_req;
    ready_req.set_context_id(101);
    ready_file.request_id = ready_req;
    ready_file.upload_status =
        contextual_search::ContextUploadStatus::kUploadSuccessful;
    uploaded_files = {ready_file};

    mock_handle->GetUploadedContextTokensForTesting() = {ready_token,
                                                         failing_token};
    mock_handle->AttachTabPendingUpload(
        ready_token, 10, GURL("https://example.com/ready"), "Ready Tab");
    user_data->OnTabContextUploadStarted(failing_token);
    mock_handle->AttachTabPendingUpload(
        failing_token, 20, GURL("https://example.com/failing"), "Failing Tab");

    content::WebContentsTester::For(web_contents_)
        ->TestDidReceiveMouseDownEvent();
    user_data->HandleOnSubmitQueryRequest("query with failing tab");
    EXPECT_TRUE(submit_responses.empty());

    // Terminal failure unblocks the pending query and sends the ready tab.
    user_data->OnContextUploadStatusChanged(
        failing_token, lens::MimeType::kHtml, failure_status,
        contextual_search::ContextUploadErrorType::kServerError);

    ASSERT_EQ(submit_responses.size(), 1u);
    const auto& resp = submit_responses[0].on_submit_query_response();
    ASSERT_EQ(resp.added_contexts_size(), 1);
    EXPECT_EQ(resp.added_contexts(0).request_id().context_id(), 101);
  }

  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(
          testing::Return(std::vector<contextual_search::FileInfo>{}));
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleOnSubmitQueryRequest_UnblocksWhenInFlightManualTabRemoved) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  // Case 1: Deleting the in-flight tab via `DeleteContext` unblocks submit.
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(token1);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token1);
  mock_handle->AttachTabPendingUpload(token1, 10, GURL("https://example.com/1"),
                                      "Tab 1");

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("delete context query");
  EXPECT_TRUE(submit_responses.empty());

  user_data->DeleteContext(token1);
  ASSERT_EQ(submit_responses.size(), 1u);
  EXPECT_EQ(
      submit_responses[0].on_submit_query_response().added_contexts_size(), 0);

  // Case 2: Removing the token via `OnTabContextRemoved` unblocks submit.
  submit_responses.clear();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(token2);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token2);
  mock_handle->AttachTabPendingUpload(token2, 20, GURL("https://example.com/2"),
                                      "Tab 2");

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("remove token query");
  EXPECT_TRUE(submit_responses.empty());

  user_data->OnTabContextRemoved(token2);
  ASSERT_EQ(submit_responses.size(), 1u);

  // Case 3: Removing the uploaded token on `session_handle` unblocks submit via
  // `UpdateContextLibraryInputState()`.
  submit_responses.clear();
  base::UnguessableToken token3 = base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(token3);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token3);
  mock_handle->AttachTabPendingUpload(token3, 30, GURL("https://example.com/3"),
                                      "Tab 3");

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("session remove token query");
  EXPECT_TRUE(submit_responses.empty());

  mock_handle->RemoveUploadedContextToken(token3);
  ASSERT_EQ(submit_responses.size(), 1u);

  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleOnSubmitQueryRequest_ClearInputPlateStateCancelsDeferredSubmit) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  base::UnguessableToken token = base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(token);
  mock_handle->GetUploadedContextTokensForTesting().push_back(token);
  mock_handle->AttachTabPendingUpload(token, 10, GURL("https://example.com/1"),
                                      "Tab 1");

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("stale query");
  EXPECT_TRUE(submit_responses.empty());

  // Clearing input plate state (e.g. on page/thread change) cancels the
  // deferred submit without sending `OnSubmitQueryResponse`.
  user_data->ClearInputPlateState();
  EXPECT_TRUE(submit_responses.empty());
  EXPECT_TRUE(mock_handle->previous_turns().empty());

  // A late upload completion for the cleared tab must not trigger the cancelled
  // submit.
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  EXPECT_TRUE(submit_responses.empty());

  // A subsequent fresh submission on the new thread succeeds immediately.
  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("fresh query");
  ASSERT_EQ(submit_responses.size(), 1u);
  ASSERT_EQ(mock_handle->previous_turns().size(), 1u);
  EXPECT_EQ(mock_handle->previous_turns()[0].query, "fresh query");

  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
}

#if !BUILDFLAG(IS_ANDROID)
namespace {

class StubTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  using CallbackHandler =
      base::RepeatingCallback<void(GetPageContextCallback callback)>;

  StubTabContextualizationController(tabs::TabInterface* tab,
                                     CallbackHandler handler)
      : lens::TabContextualizationController(tab),
        handler_(std::move(handler)) {}
  ~StubTabContextualizationController() override = default;

  void GetPageContext(GetPageContextCallback callback) override {
    ++get_page_context_calls_;
    handler_.Run(std::move(callback));
  }

  int get_page_context_calls() const { return get_page_context_calls_; }

 private:
  CallbackHandler handler_;
  int get_page_context_calls_ = 0;
};

}  // namespace

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    HandleOnSubmitQueryRequest_ActiveTabRecontextualizationSkipStillWaitsForManualUpload) {
  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host, mock_tab_list);
  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);

  content::WebContents* active_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(active_contents,
                                                   base::NullCallback());
  const GURL active_url("https://example.com/active");
  content::WebContentsTester::For(active_contents)
      ->NavigateAndCommit(active_url);
  const SessionID active_session_id =
      sessions::SessionTabHelper::IdForTab(active_contents);

  tabs::MockTabInterface mock_active_tab;
  ON_CALL(mock_active_tab, GetContents())
      .WillByDefault(testing::Return(active_contents));
  ON_CALL(mock_active_tab, GetBrowserWindowInterface())
      .WillByDefault(testing::Return(&mock_browser_window));
  std::vector<tabs::TabInterface*> all_tabs = {&mock_active_tab};
  ON_CALL(mock_tab_list, GetActiveTab())
      .WillByDefault(testing::Return(&mock_active_tab));
  ON_CALL(mock_tab_list, GetAllTabs())
      .WillByDefault(testing::ReturnPointee(&all_tabs));

  StubTabContextualizationController tab_controller(
      &mock_active_tab,
      base::BindLambdaForTesting(
          [&](lens::TabContextualizationController::GetPageContextCallback
                  callback) {
            auto data = std::make_unique<lens::ContextualInputData>();
            data->tab_session_id = active_session_id;
            data->page_url = active_url;
            data->page_title = "Active Tab";
            data->is_page_context_eligible = true;
            data->viewport_screenshot_bytes = {1, 2, 3};
            std::move(callback).Run(std::move(data));
          }));

  const base::UnguessableToken manual_token = base::UnguessableToken::Create();
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(101);

  contextual_search::FileInfo in_flight_file;
  in_flight_file.file_token = manual_token;
  in_flight_file.tab_session_id = active_session_id;
  in_flight_file.tab_url = active_url;
  in_flight_file.tab_title = "Active Tab";
  in_flight_file.request_id = req_id;
  in_flight_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadStarted;
  in_flight_file.input_data = std::make_unique<lens::ContextualInputData>();
  in_flight_file.input_data->tab_session_id = active_session_id;
  in_flight_file.input_data->page_url = active_url;
  in_flight_file.input_data->page_title = "Active Tab";
  in_flight_file.input_data->viewport_screenshot_bytes = {1, 2, 3};
  in_flight_file.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;

  testing::NiceMock<contextual_search::MockContextualSearchContextController>
      mock_context_controller;
  ON_CALL(mock_context_controller, GetFileInfoList())
      .WillByDefault([&in_flight_file]() {
        return std::vector<raw_ptr<const contextual_search::FileInfo>>{
            &in_flight_file};
      });
  ON_CALL(mock_context_controller, GetFileInfo(manual_token))
      .WillByDefault(testing::Return(&in_flight_file));

  std::vector<contextual_search::FileInfo> uploaded_files;
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("active_tab_manual_session"));
  ON_CALL(*mock_handle, GetController())
      .WillByDefault(testing::Return(&mock_context_controller));
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::ReturnPointee(&uploaded_files));
  // Recontextualization sees unchanged content and must not start a new upload.
  EXPECT_CALL(*mock_handle,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(task_id, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  testing::NiceMock<MockContextualTasksService> mock_tasks_service;
  ON_CALL(mock_tasks_service,
          GetContextForTask(task_id, testing::_, testing::_, testing::_))
      .WillByDefault(
          [task_id, active_url, active_session_id](
              const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              std::unique_ptr<ContextDecorationParams>,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) {
            ContextualTask task(task_id);
            UrlResource resource(active_url, ResourceType::kWebpage);
            resource.tab_id = active_session_id;
            task.AddUrlResource(resource);
            std::move(callback).Run(
                std::make_unique<ContextualTaskContext>(task));
          });

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->SetTaskId(task_id);
  user_data->SetContextualTasksServiceForTesting(&mock_tasks_service);
  user_data->RecordTabIdMapping(mock_active_tab.GetHandle().raw_value(),
                                active_session_id.id());
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  // Manually attach the active tab while its network upload is still in flight.
  user_data->OnTabContextUploadStarted(manual_token);
  mock_handle->GetUploadedContextTokensForTesting().push_back(manual_token);
  mock_handle->AttachTabPendingUpload(manual_token, active_session_id.id(),
                                      active_url, "Active Tab");

  // Submit while the manual upload is still in `kUploadStarted`.
  // `QueryContextualizer` recontextualizes the active tab, sees unchanged
  // content, and skips starting a new upload, but submission must remain
  // deferred until `manual_token` finishes uploading.
  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("summarize active tab");
  EXPECT_EQ(tab_controller.get_page_context_calls(), 1);
  EXPECT_TRUE(user_data->submit_pending());
  EXPECT_TRUE(submit_responses.empty());

  // Completing the in-flight manual upload sends `OnSubmitQueryResponse` with
  // the manual tab's context.
  in_flight_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  contextual_search::FileInfo completed_file;
  completed_file.file_token = manual_token;
  completed_file.request_id = req_id;
  completed_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  completed_file.input_data = std::make_unique<lens::ContextualInputData>();
  completed_file.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;
  uploaded_files.push_back(std::move(completed_file));

  user_data->OnContextUploadStatusChanged(
      manual_token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  EXPECT_FALSE(user_data->submit_pending());
  ASSERT_EQ(submit_responses.size(), 1u);
  const auto& resp = submit_responses[0].on_submit_query_response();
  ASSERT_EQ(resp.added_contexts_size(), 1);
  EXPECT_EQ(resp.added_contexts(0).request_id().context_id(), 101);

  user_data->SetContextualTasksServiceForTesting(nullptr);
  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
  ON_CALL(*mock_handle, GetController())
      .WillByDefault(testing::Return(nullptr));
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(
          testing::Return(std::vector<contextual_search::FileInfo>{}));
  webui::SetBrowserWindowInterface(web_contents_, nullptr);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       CaptureAndUploadTabContext_NullPageContextUnblocksSubmit) {
  content::WebContents* tab_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(tab_contents,
                                                   base::NullCallback());
  const GURL tab_url("https://example.com/null-context");
  content::WebContentsTester::For(tab_contents)->NavigateAndCommit(tab_url);
  const SessionID session_id =
      sessions::SessionTabHelper::IdForTab(tab_contents);

  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents()).WillByDefault(testing::Return(tab_contents));

  bool hold_callback = false;
  lens::TabContextualizationController::GetPageContextCallback held_callback;
  StubTabContextualizationController tab_controller(
      &mock_tab,
      base::BindLambdaForTesting(
          [&](lens::TabContextualizationController::GetPageContextCallback
                  callback) {
            if (hold_callback) {
              held_callback = std::move(callback);
            } else {
              std::move(callback).Run(nullptr);
            }
          }));

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, CreateContextToken()).WillByDefault([mock_handle]() {
    auto token = base::UnguessableToken::Create();
    mock_handle->GetUploadedContextTokensForTesting().push_back(token);
    return token;
  });

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));

  // 1. Synchronous null `GetPageContext` result does not leave a pending upload
  // token blocking submission.
  auto sync_result = CaptureAndUploadTabContext(
      mock_tab.GetHandle().raw_value(), mock_handle, base::DoNothing(),
      /*browser_window_interface=*/nullptr, user_data);
  ASSERT_TRUE(sync_result.has_value());
  user_data->RecordTabIdMapping(mock_tab.GetHandle().raw_value(),
                                session_id.id());
  mock_handle->AttachTabPendingUpload(*sync_result, session_id.id(), tab_url,
                                      "Null Context Tab");

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("sync null query");
  ASSERT_EQ(submit_responses.size(), 1u);

  // 2. Asynchronous null `GetPageContext` result holds submission while in
  // flight and unblocks submission as soon as `GetPageContext` finishes null.
  submit_responses.clear();
  hold_callback = true;

  auto async_result = CaptureAndUploadTabContext(
      mock_tab.GetHandle().raw_value(), mock_handle, base::DoNothing(),
      /*browser_window_interface=*/nullptr, user_data);
  ASSERT_TRUE(async_result.has_value());
  ASSERT_TRUE(held_callback);
  mock_handle->AttachTabPendingUpload(*async_result, session_id.id(), tab_url,
                                      "Null Context Tab");

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("async null query");
  EXPECT_TRUE(submit_responses.empty());

  std::move(held_callback).Run(nullptr);
  ASSERT_EQ(submit_responses.size(), 1u);

  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleOnSubmitQueryRequest_RecordsSubmitMetricsAndPanelInteraction) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockContextualTasksPanelController> mock_panel_controller(
      &mock_browser_window);
  ON_CALL(mock_panel_controller, IsPanelOpenForContextualTask())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_panel_controller, GetActiveWebContents())
      .WillByDefault(testing::Return(web_contents_));
  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);

  auto metrics_recorder =
      std::make_unique<contextual_search::ContextualSearchMetricsRecorder>(
          contextual_search::ContextualSearchSource::kOmnibox);
  metrics_recorder->NotifySessionStateChanged(
      contextual_search::SessionState::kSessionStarted);

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, GetMetricsRecorder())
      .WillByDefault(testing::Return(metrics_recorder.get()));

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle), nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();
  user_data->SetSearchMessageDispatcherForTesting(base::DoNothing());

  EXPECT_CALL(mock_panel_controller, OnAiInteraction()).Times(1);
  EXPECT_CALL(mock_panel_controller, OnQuerySubmitted()).Times(1);

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("hello world");

  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.Composebox.UserAction.QuerySubmitted"),
            1);
  EXPECT_EQ(metrics_recorder->source(),
            contextual_search::ContextualSearchSource::kContextualTasks);
  histogram_tester.ExpectUniqueSample(
      "ContextualSearch.Query.TextLength.ContextualTasks", 11, 1);

  ON_CALL(*mock_handle, GetMetricsRecorder())
      .WillByDefault(testing::Return(nullptr));
  metrics_recorder.reset();
  histogram_tester.ExpectTotalCount(
      "ContextualSearch.Session.Duration.Abandoned.ContextualTasks", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualSearch.Session.Duration.QuerySubmitted.ContextualTasks", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualSearch.Session.QueryCount.ContextualTasks", 1, 1);

  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
  webui::SetBrowserWindowInterface(web_contents_, nullptr);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       HandleOnSubmitQueryRequest_FullTabRecordsPageContextIneligibleMetric) {
  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));

  tabs::MockTabInterface mock_aim_tab;
  ON_CALL(mock_aim_tab, GetContents())
      .WillByDefault(testing::Return(web_contents_.get()));
  ON_CALL(mock_aim_tab, GetBrowserWindowInterface())
      .WillByDefault(testing::Return(&mock_browser_window));
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents_,
                                                       &mock_aim_tab);
  base::ScopedClosureRunner cleanup_tab_lookup(base::BindOnce(
      [](content::WebContents* wc) {
        wc->RemoveUserData(tabs::TabLookupFromWebContents::UserDataKey());
      },
      base::Unretained(web_contents_)));

  content::WebContents* protected_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(protected_contents,
                                                   base::NullCallback());
  const GURL protected_url("https://protected.example.com/doc");
  content::WebContentsTester::For(protected_contents)
      ->NavigateAndCommit(protected_url);
  const SessionID protected_session_id =
      sessions::SessionTabHelper::IdForTab(protected_contents);

  tabs::MockTabInterface mock_protected_tab;
  ON_CALL(mock_protected_tab, GetContents())
      .WillByDefault(testing::Return(protected_contents));
  ON_CALL(mock_protected_tab, GetBrowserWindowInterface())
      .WillByDefault(testing::Return(&mock_browser_window));

  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host, mock_tab_list);
  std::vector<tabs::TabInterface*> all_tabs = {&mock_aim_tab,
                                               &mock_protected_tab};
  ON_CALL(mock_tab_list, GetActiveTab())
      .WillByDefault(testing::Return(&mock_aim_tab));
  ON_CALL(mock_tab_list, GetAllTabs())
      .WillByDefault(testing::ReturnPointee(&all_tabs));

  StubTabContextualizationController tab_controller(
      &mock_protected_tab,
      base::BindLambdaForTesting(
          [&](lens::TabContextualizationController::GetPageContextCallback
                  callback) {
            auto data = std::make_unique<lens::ContextualInputData>();
            data->tab_session_id = protected_session_id;
            data->page_url = protected_url;
            data->page_title = "Protected Tab";
            data->is_page_context_eligible = false;
            std::move(callback).Run(std::move(data));
          }));

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->OnHandshakeComplete();
  user_data->RecordTabIdMapping(mock_protected_tab.GetHandle().raw_value(),
                                protected_session_id.id());

  std::vector<lens::ClientToSearchMessage> submit_responses;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&submit_responses](const lens::ClientToSearchMessage& message) {
        if (message.has_on_submit_query_response()) {
          submit_responses.push_back(message);
        }
      }));
  base::ScopedClosureRunner cleanup_dispatcher(base::BindOnce(
      [](ContextualTasksWebContentsUserData* ud) {
        ud->SetSearchMessageDispatcherForTesting(base::NullCallback());
      },
      base::Unretained(user_data)));

  // Simulate manually attaching a CSE-protected tab whose initial upload failed
  // page context validation.
  const base::UnguessableToken failed_manual_token =
      base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(failed_manual_token);
  mock_handle->GetUploadedContextTokensForTesting().push_back(
      failed_manual_token);
  mock_handle->AttachTabPendingUpload(failed_manual_token,
                                      protected_session_id.id(), protected_url,
                                      "Protected Tab");
  user_data->OnContextUploadStatusChanged(
      failed_manual_token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kValidationFailed, std::nullopt);

  base::HistogramTester histogram_tester;
  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("summarize protected tab");

  EXPECT_EQ(tab_controller.get_page_context_calls(), 1);
  EXPECT_FALSE(user_data->submit_pending());
  ASSERT_EQ(submit_responses.size(), 1u);
  EXPECT_EQ(
      submit_responses[0].on_submit_query_response().added_contexts_size(), 0);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.PageContextIneligible.Tab", true, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.PageContextIneligible.Panel", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualSearch.ErrorPageShown.ContextualTasks", 0);

  // Verify an unhosted WebContents (neither in a panel nor attached to a
  // TabInterface) emits neither Tab nor Panel metrics.
  content::WebContents* unhosted_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  auto unhosted_session = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* unhosted_handle = unhosted_session.get();
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      unhosted_contents)
      ->SetTaskSession(std::nullopt, std::move(unhosted_session),
                       /*input_state_model=*/nullptr);
  auto* unhosted_user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          unhosted_contents);
  ASSERT_NE(unhosted_user_data, nullptr);
  unhosted_user_data->OnHandshakeComplete();
  unhosted_user_data->RecordTabIdMapping(
      mock_protected_tab.GetHandle().raw_value(), protected_session_id.id());
  submit_responses.clear();
  unhosted_user_data->SetSearchMessageDispatcherForTesting(
      base::BindLambdaForTesting(
          [&submit_responses](const lens::ClientToSearchMessage& message) {
            if (message.has_on_submit_query_response()) {
              submit_responses.push_back(message);
            }
          }));
  base::ScopedClosureRunner cleanup_unhosted_dispatcher(base::BindOnce(
      [](ContextualTasksWebContentsUserData* ud) {
        ud->SetSearchMessageDispatcherForTesting(base::NullCallback());
      },
      base::Unretained(unhosted_user_data)));

  unhosted_handle->AddPendingDelayedTab(protected_session_id.id(),
                                        protected_url, "Protected Tab");
  content::WebContentsTester::For(unhosted_contents)
      ->TestDidReceiveMouseDownEvent();
  unhosted_user_data->HandleOnSubmitQueryRequest("summarize unhosted");

  EXPECT_EQ(tab_controller.get_page_context_calls(), 2);
  ASSERT_EQ(submit_responses.size(), 1u);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.PageContextIneligible.Tab", true, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.PageContextIneligible.Panel", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualSearch.ErrorPageShown.ContextualTasks", 0);
}

TEST_F(ContextualTasksWebContentsUserDataTest,
       ImplicitLensOverlayTabUpload_DoesNotEmitUploadingOrReadyContextState) {
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  std::vector<lens::ClientToSearchMessage> context_state_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&context_state_messages](const lens::ClientToSearchMessage& message) {
        if (message.has_on_context_state_changed()) {
          context_state_messages.push_back(message);
        }
      }));
  base::ScopedClosureRunner reset_dispatcher(base::BindOnce(
      [](ContextualTasksWebContentsUserData* ud) {
        ud->SetSearchMessageDispatcherForTesting(base::NullCallback());
      },
      user_data));
  user_data->OnHandshakeComplete();
  context_state_messages.clear();

  contextual_search::ContextualSearchContextController::
      ContextUploadStatusObserver* registered_observer = nullptr;
  std::optional<contextual_search::FileInfo> stored_file_info;
  testing::NiceMock<contextual_search::MockContextualSearchContextController>
      mock_context_controller;
  base::WeakPtrFactory<contextual_search::ContextualSearchContextController>
      controller_weak_factory(&mock_context_controller);
  ON_CALL(mock_context_controller, AsWeakPtr())
      .WillByDefault([&controller_weak_factory]() {
        return controller_weak_factory.GetWeakPtr();
      });
  EXPECT_CALL(mock_context_controller, AddObserver(testing::_))
      .WillOnce(testing::SaveArg<0>(&registered_observer));

  ON_CALL(mock_context_controller, GetFileInfo(testing::_))
      .WillByDefault([&stored_file_info](const base::UnguessableToken& token)
                         -> const contextual_search::FileInfo* {
        if (stored_file_info && stored_file_info->file_token == token) {
          return &*stored_file_info;
        }
        return nullptr;
      });
  ON_CALL(mock_context_controller,
          StartFileUploadFlow(testing::_, testing::_, testing::_))
      .WillByDefault([&](const base::UnguessableToken& file_token,
                         std::unique_ptr<lens::ContextualInputData> input_data,
                         std::optional<lens::ImageEncodingOptions>) {
        contextual_search::FileInfo info;
        info.file_token = file_token;
        info.is_implicit_upload = input_data->is_implicit_upload;
        info.upload_status =
            contextual_search::ContextUploadStatus::kProcessing;
        info.input_data = std::move(input_data);
        stored_file_info = std::move(info);
        if (registered_observer) {
          registered_observer->OnContextUploadStatusChanged(
              file_token, lens::MimeType::kAnnotatedPageContent,
              contextual_search::ContextUploadStatus::kProcessing,
              std::nullopt);
        }
      });

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, GetController())
      .WillByDefault(testing::Return(&mock_context_controller));
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault([&stored_file_info]() {
        std::vector<contextual_search::FileInfo> infos;
        if (stored_file_info) {
          contextual_search::FileInfo copy;
          copy.file_token = stored_file_info->file_token;
          copy.is_implicit_upload = stored_file_info->is_implicit_upload;
          copy.upload_status = stored_file_info->upload_status;
          infos.push_back(std::move(copy));
        }
        return infos;
      });
  ON_CALL(*mock_handle,
          StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillByDefault(
          [mock_handle](
              const base::UnguessableToken& file_token,
              std::unique_ptr<lens::ContextualInputData> contextual_input_data,
              std::optional<lens::ImageEncodingOptions> image_options) {
            mock_handle
                ->ContextualSearchSessionHandle::StartTabContextUploadFlow(
                    file_token, std::move(contextual_input_data),
                    image_options);
          });

  // Attaching the session via `SetTaskSession` (`OnSessionChanged`) subscribes
  // `user_data` to `mock_context_controller`.
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);
  base::ScopedClosureRunner clear_session(base::BindOnce(
      [](content::WebContents* wc) {
        ContextualSearchWebContentsHelper::GetOrCreateForWebContents(wc)
            ->SetTaskSession(std::nullopt, /*handle=*/nullptr,
                             /*input_state_model=*/nullptr);
      },
      web_contents_.get()));
  EXPECT_EQ(registered_observer, user_data);
  context_state_messages.clear();

  // Opening/reopening the Lens Overlay starts an implicit tab context upload.
  // Neither `StartTabContextUploadFlow` nor `kUploadSuccessful` may emit
  // `CONTEXT_STATE_UPLOADING` or `CONTEXT_STATE_READY`.
  const base::UnguessableToken overlay_token = base::UnguessableToken::Create();
  mock_handle->GetUploadedContextTokensForTesting().push_back(overlay_token);
  auto input_data = std::make_unique<lens::ContextualInputData>();
  input_data->tab_session_id = SessionID::FromSerializedValue(42);
  input_data->page_url = GURL("https://www.wikipedia.org/");
  input_data->page_title = "Wikipedia";
  input_data->is_implicit_upload = true;
  mock_handle->StartTabContextUploadFlow(overlay_token, std::move(input_data),
                                         std::nullopt);
  EXPECT_TRUE(context_state_messages.empty());

  ASSERT_TRUE(stored_file_info.has_value());
  stored_file_info->upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  ASSERT_NE(registered_observer, nullptr);
  registered_observer->OnContextUploadStatusChanged(
      overlay_token, lens::MimeType::kAnnotatedPageContent,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  EXPECT_TRUE(context_state_messages.empty());

  EXPECT_CALL(mock_context_controller, RemoveObserver(user_data)).Times(1);
  clear_session.RunAndReset();
  reset_dispatcher.RunAndReset();
}

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    HandleOnSubmitQueryRequest_RecontextualizesReopenedThreadRestoredActiveTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kContextualTasksRearchitecture, kContextualTasksContextLibrary}, {});

  ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext*) {
        return std::unique_ptr<KeyedService>(
            std::make_unique<testing::NiceMock<MockContextualTasksService>>());
      }));
  auto* mock_tasks_service = static_cast<MockContextualTasksService*>(
      ContextualTasksServiceFactory::GetForProfile(profile_.get()));
  ASSERT_NE(mock_tasks_service, nullptr);

  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host, mock_tab_list);
  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);

  content::WebContents* active_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(active_contents,
                                                   base::NullCallback());
  const GURL reopened_url("https://example.com/reopened-tab");
  content::WebContentsTester::For(active_contents)
      ->NavigateAndCommit(reopened_url);
  content::WebContentsTester::For(active_contents)->SetTitle(u"Reopened Tab");
  const SessionID active_session_id =
      sessions::SessionTabHelper::IdForTab(active_contents);

  tabs::MockTabInterface mock_active_tab;
  ON_CALL(mock_active_tab, GetContents())
      .WillByDefault(testing::Return(active_contents));
  ON_CALL(mock_active_tab, GetTitle())
      .WillByDefault(testing::Return(u"Reopened Tab"));
  ON_CALL(mock_active_tab, GetBrowserWindowInterface())
      .WillByDefault(testing::Return(&mock_browser_window));
  std::vector<tabs::TabInterface*> all_tabs = {&mock_active_tab};
  ON_CALL(mock_tab_list, GetActiveTab())
      .WillByDefault(testing::Return(&mock_active_tab));
  ON_CALL(mock_tab_list, GetAllTabs())
      .WillByDefault(testing::ReturnPointee(&all_tabs));

  StubTabContextualizationController tab_controller(
      &mock_active_tab,
      base::BindLambdaForTesting(
          [active_session_id, active_contents](
              lens::TabContextualizationController::GetPageContextCallback
                  callback) {
            auto data = std::make_unique<lens::ContextualInputData>();
            data->tab_session_id = active_session_id;
            data->page_url = active_contents->GetLastCommittedURL();
            data->page_title = "Reopened Tab";
            data->is_page_context_eligible = true;
            data->viewport_screenshot_bytes = {4, 5, 6};
            std::move(callback).Run(std::move(data));
          }));

  contextual_search::ContextualSearchContextController::
      ContextUploadStatusObserver* captured_observer = nullptr;
  testing::NiceMock<contextual_search::MockContextualSearchContextController>
      mock_context_controller;
  base::WeakPtrFactory<contextual_search::ContextualSearchContextController>
      controller_weak_factory(&mock_context_controller);
  ON_CALL(mock_context_controller, AsWeakPtr())
      .WillByDefault([&controller_weak_factory]() {
        return controller_weak_factory.GetWeakPtr();
      });
  ON_CALL(mock_context_controller, AddObserver(testing::_))
      .WillByDefault([&captured_observer](
                         contextual_search::ContextualSearchContextController::
                             ContextUploadStatusObserver* obs) {
        captured_observer = obs;
      });
  ON_CALL(mock_context_controller, RemoveObserver(testing::_))
      .WillByDefault([&captured_observer](
                         contextual_search::ContextualSearchContextController::
                             ContextUploadStatusObserver* obs) {
        if (captured_observer == obs) {
          captured_observer = nullptr;
        }
      });

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, GetController())
      .WillByDefault(testing::Return(&mock_context_controller));
  ON_CALL(*mock_handle, CreateContextToken()).WillByDefault([]() {
    return base::UnguessableToken::Create();
  });
  int recontextualization_uploads = 0;
  ON_CALL(*mock_handle,
          StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillByDefault([&recontextualization_uploads, &captured_observer](
                         const base::UnguessableToken& file_token,
                         std::unique_ptr<lens::ContextualInputData> data,
                         std::optional<lens::ImageEncodingOptions>) {
        ++recontextualization_uploads;
        EXPECT_EQ(data->upload_type,
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_RECONTEXTUALIZATION);
        if (captured_observer) {
          captured_observer->OnContextUploadStatusChanged(
              file_token, lens::MimeType::kUnknown,
              contextual_search::ContextUploadStatus::kUploadSuccessful,
              std::nullopt);
        }
      });

  const base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(task_id, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  ContextualTask stored_task(task_id);
  ON_CALL(*mock_tasks_service, SetUrlResourcesFromServer(task_id, testing::_))
      .WillByDefault([&stored_task](const base::Uuid&,
                                    std::vector<UrlResource> resources) {
        stored_task.SetUrlResourcesFromServer(std::move(resources));
      });
  ON_CALL(*mock_tasks_service,
          GetContextForTask(task_id, testing::_, testing::_, testing::_))
      .WillByDefault(
          [&stored_task](
              const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              std::unique_ptr<ContextDecorationParams>,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) {
            std::move(callback).Run(
                std::make_unique<ContextualTaskContext>(stored_task));
          });

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->SetTaskId(task_id);
  user_data->OnHandshakeComplete();
  user_data->SetSearchMessageDispatcherForTesting(base::DoNothing());

  // Reopen the thread: AIM sends committed tab contexts while
  // `submitted_context` on `session_handle` is empty.
  lens::SearchToClientMessage::UpdateThreadContextLibrary update;
  auto* open_tab_ctx = update.add_contexts();
  open_tab_ctx->set_context_id(301);
  open_tab_ctx->set_has_chrome_tab_data(true);
  open_tab_ctx->mutable_webpage()->set_url(reopened_url.spec());
  open_tab_ctx->mutable_webpage()->set_title("Reopened Tab");

  const GURL closed_url("https://example.com/closed-tab");
  auto* closed_tab_ctx = update.add_contexts();
  closed_tab_ctx->set_context_id(302);
  closed_tab_ctx->set_has_chrome_tab_data(true);
  closed_tab_ctx->mutable_webpage()->set_url(closed_url.spec());
  closed_tab_ctx->mutable_webpage()->set_title("Closed Tab");

  user_data->HandleThreadContextLibraryUpdateFromAim(update);
  ASSERT_TRUE(mock_handle->GetTabContextState().ContainsTab(active_session_id));

  // 1. Submitting on the reopened thread recontextualizes the open restored
  // active tab because `HandleThreadContextLibraryUpdateFromAim` populated both
  // `restored` tabs and `UrlResource::tab_id`.
  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("follow up on reopened thread");
  EXPECT_EQ(tab_controller.get_page_context_calls(), 1);
  EXPECT_EQ(recontextualization_uploads, 1);

  // 2. Deselecting the restored tab stops recontextualization on submit.
  mock_handle->DeleteTabContext(active_session_id, reopened_url,
                                "Reopened Tab");
  ASSERT_TRUE(mock_handle->IsTabDeselected(active_session_id, reopened_url,
                                           "Reopened Tab"));
  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("after deselect");
  EXPECT_EQ(tab_controller.get_page_context_calls(), 1);
  EXPECT_EQ(recontextualization_uploads, 1);

  // 3. Navigating to an unaffiliated URL (or a URL that was closed when the
  // thread reopened and therefore excluded from `restored`) does not
  // recontextualize.
  content::WebContentsTester::For(active_contents)
      ->NavigateAndCommit(closed_url);
  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("on previously closed url");
  EXPECT_EQ(tab_controller.get_page_context_calls(), 1);
  EXPECT_EQ(recontextualization_uploads, 1);

  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
  ON_CALL(*mock_handle, GetController())
      .WillByDefault(testing::Return(nullptr));
  webui::SetBrowserWindowInterface(web_contents_, nullptr);
  web_contents_ = nullptr;
  web_contents_factory_.reset();
}

TEST_F(
    ContextualTasksWebContentsUserDataTest,
    HandleOnSubmitQueryRequest_UntaskedZeroStateOnlyUploadsAutoSuggestedTabs) {
  ui::UnownedUserDataHost unowned_user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
  testing::NiceMock<MockTabListInterface> mock_tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host, mock_tab_list);
  webui::SetBrowserWindowInterface(web_contents_, &mock_browser_window);

  // Active tab in the browser window (unaffiliated with any task).
  content::WebContents* active_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(active_contents,
                                                   base::NullCallback());
  const GURL active_url("https://example.com/unaffiliated-active");
  content::WebContentsTester::For(active_contents)
      ->NavigateAndCommit(active_url);
  const SessionID active_session_id =
      sessions::SessionTabHelper::IdForTab(active_contents);

  tabs::MockTabInterface mock_active_tab;
  ON_CALL(mock_active_tab, GetContents())
      .WillByDefault(testing::Return(active_contents));
  ON_CALL(mock_active_tab, GetBrowserWindowInterface())
      .WillByDefault(testing::Return(&mock_browser_window));

  // Auto-suggested delayed chip tab.
  content::WebContents* chip_contents =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(chip_contents,
                                                   base::NullCallback());
  const GURL chip_url("https://example.com/auto-chip");
  content::WebContentsTester::For(chip_contents)->NavigateAndCommit(chip_url);
  const SessionID chip_session_id =
      sessions::SessionTabHelper::IdForTab(chip_contents);

  tabs::MockTabInterface mock_chip_tab;
  ON_CALL(mock_chip_tab, GetContents())
      .WillByDefault(testing::Return(chip_contents));
  ON_CALL(mock_chip_tab, GetBrowserWindowInterface())
      .WillByDefault(testing::Return(&mock_browser_window));

  std::vector<tabs::TabInterface*> all_tabs = {&mock_active_tab,
                                               &mock_chip_tab};
  ON_CALL(mock_tab_list, GetActiveTab())
      .WillByDefault(testing::Return(&mock_active_tab));
  ON_CALL(mock_tab_list, GetAllTabs())
      .WillByDefault(testing::ReturnPointee(&all_tabs));

  StubTabContextualizationController active_tab_controller(
      &mock_active_tab,
      base::BindLambdaForTesting(
          [active_session_id, active_url](
              lens::TabContextualizationController::GetPageContextCallback
                  callback) {
            auto data = std::make_unique<lens::ContextualInputData>();
            data->tab_session_id = active_session_id;
            data->page_url = active_url;
            data->is_page_context_eligible = true;
            std::move(callback).Run(std::move(data));
          }));
  StubTabContextualizationController chip_tab_controller(
      &mock_chip_tab,
      base::BindLambdaForTesting(
          [chip_session_id, chip_url](
              lens::TabContextualizationController::GetPageContextCallback
                  callback) {
            auto data = std::make_unique<lens::ContextualInputData>();
            data->tab_session_id = chip_session_id;
            data->page_url = chip_url;
            data->is_page_context_eligible = true;
            std::move(callback).Run(std::move(data));
          }));

  contextual_search::ContextualSearchContextController::
      ContextUploadStatusObserver* captured_observer = nullptr;
  testing::NiceMock<contextual_search::MockContextualSearchContextController>
      mock_context_controller;
  base::WeakPtrFactory<contextual_search::ContextualSearchContextController>
      controller_weak_factory(&mock_context_controller);
  ON_CALL(mock_context_controller, AsWeakPtr())
      .WillByDefault([&controller_weak_factory]() {
        return controller_weak_factory.GetWeakPtr();
      });
  ON_CALL(mock_context_controller, AddObserver(testing::_))
      .WillByDefault([&captured_observer](
                         contextual_search::ContextualSearchContextController::
                             ContextUploadStatusObserver* obs) {
        captured_observer = obs;
      });
  ON_CALL(mock_context_controller, RemoveObserver(testing::_))
      .WillByDefault([&captured_observer](
                         contextual_search::ContextualSearchContextController::
                             ContextUploadStatusObserver* obs) {
        if (captured_observer == obs) {
          captured_observer = nullptr;
        }
      });

  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();
  ON_CALL(*mock_handle, GetController())
      .WillByDefault(testing::Return(&mock_context_controller));
  ON_CALL(*mock_handle, CreateContextToken()).WillByDefault([]() {
    return base::UnguessableToken::Create();
  });

  std::vector<GURL> uploaded_tab_urls;
  ON_CALL(*mock_handle,
          StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillByDefault([&uploaded_tab_urls, &captured_observer](
                         const base::UnguessableToken& file_token,
                         std::unique_ptr<lens::ContextualInputData> data,
                         std::optional<lens::ImageEncodingOptions>) {
        ASSERT_TRUE(data->page_url.has_value());
        uploaded_tab_urls.push_back(*data->page_url);
        EXPECT_EQ(data->upload_type,
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_AUTO_TAB_CHIP);
        if (captured_observer) {
          captured_observer->OnContextUploadStatusChanged(
              file_token, lens::MimeType::kUnknown,
              contextual_search::ContextUploadStatus::kUploadSuccessful,
              std::nullopt);
        }
      });

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(/*task_id=*/std::nullopt, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  testing::NiceMock<MockContextualTasksService> mock_tasks_service;
  EXPECT_CALL(mock_tasks_service,
              GetContextForTask(testing::_, testing::_, testing::_, testing::_))
      .Times(0);

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->SetContextualTasksServiceForTesting(&mock_tasks_service);
  user_data->RecordTabIdMapping(mock_active_tab.GetHandle().raw_value(),
                                active_session_id.id());
  user_data->RecordTabIdMapping(mock_chip_tab.GetHandle().raw_value(),
                                chip_session_id.id());
  user_data->OnHandshakeComplete();
  user_data->SetSearchMessageDispatcherForTesting(base::DoNothing());

  mock_handle->AddPendingDelayedTab(chip_session_id.id(), chip_url, "Chip Tab");

  content::WebContentsTester::For(web_contents_)
      ->TestDidReceiveMouseDownEvent();
  user_data->HandleOnSubmitQueryRequest("zero state submit");

  EXPECT_EQ(active_tab_controller.get_page_context_calls(), 0);
  EXPECT_EQ(chip_tab_controller.get_page_context_calls(), 1);
  EXPECT_THAT(uploaded_tab_urls, testing::ElementsAre(chip_url));

  user_data->SetSearchMessageDispatcherForTesting(base::NullCallback());
  user_data->SetContextualTasksServiceForTesting(nullptr);
  ON_CALL(*mock_handle, GetController())
      .WillByDefault(testing::Return(nullptr));
  webui::SetBrowserWindowInterface(web_contents_, nullptr);
}
#endif

}  // namespace contextual_tasks
