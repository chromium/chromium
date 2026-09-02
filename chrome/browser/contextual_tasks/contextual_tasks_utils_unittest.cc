// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/aim_message_poster.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace contextual_tasks {

namespace {

class ContextualTasksUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    InitializeActionIdStringMapping();
    profile_ = std::make_unique<TestingProfile>();
    browser_window_ = std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();

    ON_CALL(*browser_window_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));

    browser_actions_ = std::make_unique<BrowserActions>(browser_window_.get());
  }

  void TearDown() override {
    if (browser_actions_) {
      browser_actions_->set_root_action_item_for_testing(nullptr);
    }
    browser_actions_.reset();
    actions::ActionIdMap::ResetMapsForTesting();
    actions::ActionManager::Get().ResetForTesting();
    actions::ActionIdMap::ResetMapsForTesting();
    browser_window_.reset();
    profile_.reset();
    root_action_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>> browser_window_;
  std::unique_ptr<BrowserActions> browser_actions_;
  std::unique_ptr<actions::ActionItem> root_action_;
};

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_NullWindow) {
  // Should return early and not crash.
  UpdatePinButtonVisibilityState(nullptr);
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_NullActions) {
  UpdatePinButtonVisibilityState(browser_window_.get());
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_NullRootActionItem) {
  // browser_actions_ already has a null root_action_item_ initially.
  UpdatePinButtonVisibilityState(browser_window_.get());
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_ActionNotFound) {
  // Root action item with no children (no contextual tasks action item).
  root_action_ = actions::ActionItem::Builder().Build();

  browser_actions_->set_root_action_item_for_testing(root_action_.get());

  UpdatePinButtonVisibilityState(browser_window_.get());
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_Eligible_Pinned) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{contextual_tasks::
                                kEnableContextualTasksPinButtonInToolbar,
                            contextual_tasks::
                                kContextualTasksForceEntryPointEligibility},
      /*disabled_features=*/{});

  root_action_ = actions::ActionItem::Builder().Build();
  actions::ActionItem* action_item = root_action_->AddChild(
      actions::ActionItem::Builder()
          .SetActionId(kActionSidePanelShowContextualTasks)
          .SetVisible(false)
          .SetEnabled(true)
          .Build());

  browser_actions_->set_root_action_item_for_testing(root_action_.get());

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  ASSERT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));

  UpdatePinButtonVisibilityState(browser_window_.get());

  EXPECT_TRUE(action_item->GetVisible());
  EXPECT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));
}

TEST_F(ContextualTasksUtilsTest,
       UpdatePinButtonVisibilityState_Eligible_Pinned_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{contextual_tasks::
                                kContextualTasksForceEntryPointEligibility},
      /*disabled_features=*/{contextual_tasks::
                                 kEnableContextualTasksPinButtonInToolbar});

  root_action_ = actions::ActionItem::Builder().Build();
  actions::ActionItem* action_item = root_action_->AddChild(
      actions::ActionItem::Builder()
          .SetActionId(kActionSidePanelShowContextualTasks)
          .SetVisible(true)
          .SetEnabled(true)
          .Build());

  browser_actions_->set_root_action_item_for_testing(root_action_.get());

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  ASSERT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));

  UpdatePinButtonVisibilityState(browser_window_.get());

  EXPECT_FALSE(action_item->GetVisible());
  EXPECT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_Ineligible_Pinned) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      contextual_tasks::kEnableContextualTasksPinButtonInToolbar);

  root_action_ = actions::ActionItem::Builder().Build();
  actions::ActionItem* action_item = root_action_->AddChild(
      actions::ActionItem::Builder()
          .SetActionId(kActionSidePanelShowContextualTasks)
          .SetVisible(true)
          .SetEnabled(true)
          .Build());

  browser_actions_->set_root_action_item_for_testing(root_action_.get());

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  ASSERT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));

  UpdatePinButtonVisibilityState(browser_window_.get());

  EXPECT_FALSE(action_item->GetVisible());
  EXPECT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_Ineligible_Unpinned) {
  root_action_ = actions::ActionItem::Builder().Build();
  actions::ActionItem* action_item = root_action_->AddChild(
      actions::ActionItem::Builder()
          .SetActionId(kActionSidePanelShowContextualTasks)
          .SetVisible(true)
          .SetEnabled(true)
          .Build());

  browser_actions_->set_root_action_item_for_testing(root_action_.get());

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  ASSERT_FALSE(model->Contains(kActionSidePanelShowContextualTasks));

  // Should NOT hide the action item because it is unpinned.
  UpdatePinButtonVisibilityState(browser_window_.get());

  EXPECT_TRUE(action_item->GetVisible());
  EXPECT_FALSE(model->Contains(kActionSidePanelShowContextualTasks));
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_Ineligible_Pinned_IncognitoProfile) {
  root_action_ = actions::ActionItem::Builder().Build();
  actions::ActionItem* action_item = root_action_->AddChild(
      actions::ActionItem::Builder()
          .SetActionId(kActionSidePanelShowContextualTasks)
          .SetVisible(true)
          .SetEnabled(true)
          .Build());

  browser_actions_->set_root_action_item_for_testing(root_action_.get());

  // Setup original profile and OTR profile
  Profile* otr_profile = profile_->GetPrimaryOTRProfile(true);
  ON_CALL(*browser_window_, GetProfile())
      .WillByDefault(testing::Return(otr_profile));

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  ASSERT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));

  // Should NOT hide the action item because we are in an incognito window.
  UpdatePinButtonVisibilityState(browser_window_.get());

  EXPECT_TRUE(action_item->GetVisible());
  EXPECT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));
}

TEST_F(ContextualTasksUtilsTest, IsTabSharingEligible_NullProfile) {
  EXPECT_FALSE(IsTabSharingEligible(nullptr));
}

TEST_F(ContextualTasksUtilsTest, IsTabSharingEligible_OffTheRecord) {
  Profile* otr_profile = profile_->GetPrimaryOTRProfile(true);
  EXPECT_FALSE(IsTabSharingEligible(otr_profile));
}

TEST_F(ContextualTasksUtilsTest, IsTabSharingEligible_NoAimService) {
  EXPECT_FALSE(IsTabSharingEligible(profile_.get()));
}

TEST_F(ContextualTasksUtilsTest, IsTabSharingEligible_AimIneligible) {
  testing::NiceMock<MockAimEligibilityService>* mock_aim_service = nullptr;
  AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating(
          [](testing::NiceMock<MockAimEligibilityService>** mock_out,
             content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> {
            auto mock =
                std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
                    *Profile::FromBrowserContext(context)->GetPrefs(), nullptr,
                    nullptr, nullptr);
            *mock_out = mock.get();
            return mock;
          },
          &mock_aim_service));

  AimEligibilityServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(mock_aim_service);

  ON_CALL(*mock_aim_service, IsAimEligible())
      .WillByDefault(testing::Return(false));
  ON_CALL(*mock_aim_service, IsFuseboxEligible())
      .WillByDefault(testing::Return(true));

  EXPECT_FALSE(IsTabSharingEligible(profile_.get()));
}

TEST_F(ContextualTasksUtilsTest, IsTabSharingEligible_FuseboxIneligible) {
  testing::NiceMock<MockAimEligibilityService>* mock_aim_service = nullptr;
  AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating(
          [](testing::NiceMock<MockAimEligibilityService>** mock_out,
             content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> {
            auto mock =
                std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
                    *Profile::FromBrowserContext(context)->GetPrefs(), nullptr,
                    nullptr, nullptr);
            *mock_out = mock.get();
            return mock;
          },
          &mock_aim_service));

  AimEligibilityServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(mock_aim_service);

  ON_CALL(*mock_aim_service, IsAimEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_service, IsFuseboxEligible())
      .WillByDefault(testing::Return(false));

  EXPECT_FALSE(IsTabSharingEligible(profile_.get()));
}

TEST_F(ContextualTasksUtilsTest,
       IsTabSharingEligible_FuseboxEligible_CobrowseIneligible) {
  testing::NiceMock<MockAimEligibilityService>* mock_aim_service = nullptr;
  AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating(
          [](testing::NiceMock<MockAimEligibilityService>** mock_out,
             content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> {
            auto mock =
                std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
                    *Profile::FromBrowserContext(context)->GetPrefs(), nullptr,
                    nullptr, nullptr);
            *mock_out = mock.get();
            return mock;
          },
          &mock_aim_service));

  AimEligibilityServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(mock_aim_service);

  ON_CALL(*mock_aim_service, IsAimEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_service, IsFuseboxEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_service, IsCobrowseEligible())
      .WillByDefault(testing::Return(false));

  EXPECT_TRUE(IsTabSharingEligible(profile_.get()));
}

TEST_F(ContextualTasksUtilsTest,
       IsTabSharingEligible_ForceEntryPointEligibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kContextualTasksForceEntryPointEligibility);
  EXPECT_TRUE(IsTabSharingEligible(nullptr));
  EXPECT_TRUE(IsTabSharingEligible(profile_.get()));
}

TEST_F(ContextualTasksUtilsTest, ShouldUseDarkMode_NullProfile) {
  EXPECT_FALSE(ShouldUseDarkMode(nullptr));
  EXPECT_FALSE(ShouldUseDarkMode(nullptr, GURL()));
}

TEST_F(ContextualTasksUtilsTest, ShouldUseDarkMode_UrlParamOverridesProfile) {
  GURL dark_url("https://example.com/search?cs=1");
  GURL light_url("https://example.com/search?cs=0");

  EXPECT_TRUE(ShouldUseDarkMode(profile_.get(), dark_url));
  EXPECT_FALSE(ShouldUseDarkMode(profile_.get(), light_url));

  Profile* otr_profile = profile_->GetPrimaryOTRProfile(true);
  EXPECT_TRUE(ShouldUseDarkMode(otr_profile, dark_url));
  EXPECT_FALSE(ShouldUseDarkMode(otr_profile, light_url));
}

TEST_F(ContextualTasksUtilsTest, ShouldUseDarkMode_IncognitoProfile) {
  Profile* otr_profile = profile_->GetPrimaryOTRProfile(true);
  EXPECT_TRUE(ShouldUseDarkMode(otr_profile));
  EXPECT_TRUE(ShouldUseDarkMode(otr_profile, GURL("https://example.com")));
}

class MockAimMessagePoster : public AimMessagePoster {
 public:
  MOCK_METHOD(void,
              PostAimMessage,
              (const lens::ClientToAimMessage&),
              (override));
};

TEST_F(ContextualTasksUtilsTest,
       PrepareClientToAimRequestInfo_PreservesTokenOrder) {
  testing::NiceMock<MockAimMessagePoster> message_poster;
  testing::NiceMock<contextual_search::MockContextualSearchContextController>
      mock_controller;
  testing::NiceMock<contextual_search::MockContextualSearchSessionHandle>
      mock_session;

  ON_CALL(mock_session, GetController())
      .WillByDefault(testing::Return(&mock_controller));

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  base::UnguessableToken token3 = base::UnguessableToken::Create();
  base::UnguessableToken overlay_token = base::UnguessableToken::Create();

  mock_session.GetUploadedContextTokensForTesting() = {token1, token2, token3};

  auto request_info = PrepareClientToAimRequestInfo(
      "test query", &mock_session, &message_poster,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      omnibox::ModelMode::MODEL_MODE_UNSPECIFIED,
      /*active_tab_context_id=*/std::nullopt, overlay_token,
      /*is_voice_search=*/false, /*additional_cgi_params=*/{});

  ASSERT_EQ(request_info->file_tokens.size(), 4u);
  EXPECT_EQ(request_info->file_tokens[0], token1);
  EXPECT_EQ(request_info->file_tokens[1], token2);
  EXPECT_EQ(request_info->file_tokens[2], token3);
  EXPECT_EQ(request_info->file_tokens[3], overlay_token);
}

}  // namespace

}  // namespace contextual_tasks
