// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/signin/public/base/test_signin_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TestSigninClient>(profile->GetPrefs());
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
      *mock_handle, config, GURL(), false, false);

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
      {{account_info.email, account_info.gaia}});

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
      {{account_info.email, account_info.gaia}});

  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  auto mock_handle =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();

  auto model_weak = user_data->GetOrCreateInputStateModel(*mock_handle);
  ASSERT_TRUE(model_weak);
  EXPECT_FALSE(model_weak->browser_identity_matches_aim_identity_for_testing());

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

}  // namespace contextual_tasks
