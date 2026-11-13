// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/glic/browser_ui/glic_actor_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_ui_state.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_activity_manager.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/mock_glic_instance_coordinator.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

class TestingGlicInstanceCoordinator
    : public glic::MockGlicInstanceCoordinator {
 public:
  base::CallbackListSubscription AddGlobalShowHideCallback(
      base::RepeatingClosure callback) override {
    return global_show_hide_subscriptions_.Add(std::move(callback));
  }

  void NotifyShowHide() { global_show_hide_subscriptions_.Notify(); }

 private:
  base::RepeatingClosureList global_show_hide_subscriptions_;
};

// We subclass here since we mock functions that need to return references
// to concrete instances that not all unit tests may want to provide.
class MockGlicKeyedServiceForButtonController : public MockGlicKeyedService {
 public:
  MockGlicKeyedServiceForButtonController(
      content::BrowserContext* browser_context,
      signin::IdentityManager* identity_manager,
      ProfileManager* profile_manager,
      GlicProfileManager* glic_profile_manager,
      glic::ContextualCueingService* contextual_cueing_service,
      actor::ActorKeyedService* actor_keyed_service)
      : MockGlicKeyedService(browser_context,
                             identity_manager,
                             profile_manager,
                             glic_profile_manager,
                             contextual_cueing_service,
                             actor_keyed_service),
        window_controller_(std::make_unique<TestingGlicInstanceCoordinator>()) {
  }
  MOCK_METHOD(void, TryPreload, (GlicWarmingTrigger), (override));

  bool IsPanelShowingForBrowser(
      const BrowserWindowInterface& bwi) const override {
    return browser_with_open_panel_ == &bwi;
  }

  GlicInstanceCoordinator& instance_coordinator() const override {
    return *window_controller_;
  }

  TestingGlicInstanceCoordinator& instance_coordinator_mock() {
    return *window_controller_;
  }

  // For these tests, pretend only one browser can show the panel at a time.
  void SimulatePanelShownForBrowser(BrowserWindowInterface* bwi) {
    browser_with_open_panel_ = bwi;
    window_controller_->NotifyShowHide();
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_with_open_panel_ = nullptr;
  std::unique_ptr<TestingGlicInstanceCoordinator> window_controller_;
};

class MockGlicSplitButtonDelegate : public glic::GlicSplitButtonDelegate {
 public:
  void SetGlicShowState(bool show) override { show_state_ = show; }
  void SetGlicPanelIsOpen(bool panel_open) override {
    panel_open_ = panel_open;
  }
  void UpdateGlicSplitButtonUi(const GlicSplitButtonUiState& state) override {
    last_ui_state_ = state;
  }
  bool GetIsShowingGlicNudge() override { return is_showing_nudge_; }
  void ShowActivityListBubble() override {
    is_bubble_showing_ = true;
    ++show_bubble_count_;
  }
  void CloseActivityListBubble() override { is_bubble_showing_ = false; }
  bool IsActivityListBubbleShowing() override { return is_bubble_showing_; }

  void set_is_showing_nudge(bool showing) { is_showing_nudge_ = showing; }
  bool show_state() const { return show_state_; }
  bool panel_open() const { return panel_open_; }
  bool is_bubble_showing() const { return is_bubble_showing_; }
  int show_bubble_count() const { return show_bubble_count_; }
  const std::optional<GlicSplitButtonUiState>& last_ui_state() const {
    return last_ui_state_;
  }

 private:
  bool show_state_ = false;
  bool panel_open_ = false;
  bool is_showing_nudge_ = false;
  bool is_bubble_showing_ = false;
  int show_bubble_count_ = 0;
  std::optional<GlicSplitButtonUiState> last_ui_state_;
};

}  // namespace

class GlicSplitButtonControllerTest : public testing::Test {
 public:
  GlicSplitButtonControllerTest() = default;

  void SetUp() override {
    // Enable kGlic by default for testing.
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kGlicRollout,
            features::kGlicActivityHub,
            features::kGlicActor,
            features::kGlicActorPolicyControlExemption,
        },
        {
            features::kGlicActorUi,
        });

    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

#if BUILDFLAG(IS_CHROMEOS)
    // glic can run only in User session, so it needs to set up user session
    // manually on ChromeOS.
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    TestingProfile::TestingFactories factories;
    factories.emplace_back(
        actor::ActorKeyedServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<actor::ActorKeyedServiceFake>(
              static_cast<Profile*>(context));
        }));
    factories.emplace_back(
        GlicKeyedServiceFactory::GetInstance(),
        base::BindRepeating(
            [](GlicSplitButtonControllerTest* test,
               TestingProfileManager* testing_profile_manager,
               content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              auto service =
                  std::make_unique<MockGlicKeyedServiceForButtonController>(
                      context,
                      test->identity_test_environment.identity_manager(),
                      testing_profile_manager->profile_manager(),
                      &test->glic_profile_manager_,
                      /*contextual_cueing_service=*/nullptr,
                      actor::ActorKeyedServiceFactory::GetActorKeyedService(
                          static_cast<Profile*>(context)));
              test->mock_glic_service_ = service.get();
              return service;
            },
            base::Unretained(this), testing_profile_manager));

    profile_ = testing_profile_manager->CreateTestingProfile(
        "profile", std::move(factories));

    GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);

    mock_browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_));
    ON_CALL(*mock_browser_window_interface_, IsActive())
        .WillByDefault(testing::Return(true));

    mock_tab_list_ =
        std::make_unique<testing::NiceMock<MockTabListInterface>>();
    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            mock_browser_window_interface_->GetUnownedUserDataHost(),
            *mock_tab_list_);

    histograms_ = std::make_unique<base::HistogramTester>();

    glic_split_button_controller_ = std::make_unique<GlicSplitButtonController>(
        mock_browser_window_interface_.get(), mock_glic_service_);
    glic_split_button_controller_->SetHorizontalTabsDelegate(
        &mock_tab_strip_glic_controller_delegate_);
    glic_split_button_controller_->SetVerticalTabsDelegate(
        &mock_toolbar_glic_controller_delegate_);

    glic_test_env_.SetupProfile(profile());

    PrefService* prefs = profile()->GetPrefs();
    prefs->SetInteger(
        optimization_guide::prefs::kGeminiSettings,
        std::to_underlying(
            optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
    prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
  }

  void TearDown() override {
    glic_split_button_controller_.reset();
    tab_list_registration_.reset();
    mock_tab_list_.reset();
    mock_browser_window_interface_.reset();

    mock_glic_service_ = nullptr;
    profile_ = nullptr;

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
    scoped_feature_list_.Reset();
  }

  GlicSplitButtonController* controller() {
    return GlicSplitButtonController::From(browser_window_interface());
  }

  MockGlicSplitButtonDelegate* tab_strip_controller_delegate() {
    return &mock_tab_strip_glic_controller_delegate_;
  }

  MockGlicSplitButtonDelegate* toolbar_controller_delegate() {
    return &mock_toolbar_glic_controller_delegate_;
  }

  base::HistogramTester& histograms() { return *histograms_; }

  void reset_controller() { glic_split_button_controller_.reset(); }

  Profile* profile() { return profile_; }
  MockGlicKeyedServiceForButtonController* glic_keyed_service() {
    return mock_glic_service_;
  }
  BrowserWindowInterface* browser_window_interface() {
    return mock_browser_window_interface_.get();
  }
  MockTabListInterface* tab_list() { return mock_tab_list_.get(); }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  GlicUnitTestEnvironment glic_test_env_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

#if BUILDFLAG(IS_CHROMEOS)
  // glic can run only in User session, so it needs to set up user session
  // manually on ChromeOS.
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<Profile> profile_ = nullptr;
  signin::IdentityTestEnvironment identity_test_environment;

  GlicProfileManager glic_profile_manager_;
  MockGlicSplitButtonDelegate mock_tab_strip_glic_controller_delegate_;
  MockGlicSplitButtonDelegate mock_toolbar_glic_controller_delegate_;
  std::unique_ptr<base::HistogramTester> histograms_;
  std::unique_ptr<actor::ActorKeyedServiceFake> actor_keyed_service_;
  raw_ptr<MockGlicKeyedServiceForButtonController> mock_glic_service_ = nullptr;
  std::unique_ptr<GlicSplitButtonController> glic_split_button_controller_;
  std::unique_ptr<MockTabListInterface> mock_tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  std::unique_ptr<MockBrowserWindowInterface> mock_browser_window_interface_;
};

// Test that settings changes are reflected in the show state of the controller
// delegate.
TEST_F(GlicSplitButtonControllerTest, GlicSettings) {
  PrefService* prefs = profile()->GetPrefs();

  EXPECT_TRUE(tab_strip_controller_delegate()->show_state());
  EXPECT_TRUE(toolbar_controller_delegate()->show_state());

  prefs->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  EXPECT_FALSE(tab_strip_controller_delegate()->show_state());
  EXPECT_FALSE(toolbar_controller_delegate()->show_state());

  prefs->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
  EXPECT_TRUE(tab_strip_controller_delegate()->show_state());
  EXPECT_TRUE(toolbar_controller_delegate()->show_state());

  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
  EXPECT_FALSE(tab_strip_controller_delegate()->show_state());
  EXPECT_FALSE(toolbar_controller_delegate()->show_state());

  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
  EXPECT_TRUE(tab_strip_controller_delegate()->show_state());
  EXPECT_TRUE(toolbar_controller_delegate()->show_state());

  prefs->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
  EXPECT_FALSE(tab_strip_controller_delegate()->show_state());
  EXPECT_FALSE(toolbar_controller_delegate()->show_state());
}

TEST_F(GlicSplitButtonControllerTest, PanelStateChangedSameBrowser) {
  EXPECT_TRUE(tab_strip_controller_delegate()->show_state());
  EXPECT_TRUE(toolbar_controller_delegate()->show_state());
  EXPECT_FALSE(tab_strip_controller_delegate()->panel_open());
  EXPECT_FALSE(toolbar_controller_delegate()->panel_open());

  glic_keyed_service()->SimulatePanelShownForBrowser(
      browser_window_interface());
  EXPECT_TRUE(tab_strip_controller_delegate()->panel_open());
  EXPECT_TRUE(toolbar_controller_delegate()->panel_open());

  glic_keyed_service()->SimulatePanelShownForBrowser(nullptr);
  EXPECT_FALSE(tab_strip_controller_delegate()->panel_open());
  EXPECT_FALSE(toolbar_controller_delegate()->panel_open());
}

TEST_F(GlicSplitButtonControllerTest, RecordStartupMetrics) {
  // Initial state: IsEnabled() is false because of lack sign in.
  histograms().ExpectUniqueSample("Glic.ProfileEnablement.IsEnabled.Startup",
                                  false, 1);

  // Updating the 'PinnedToTabstrip' pref triggers another call to
  // MaybeRecordStartupMetrics.
  profile()->GetPrefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
  EXPECT_FALSE(tab_strip_controller_delegate()->show_state());

  // Startup metrics should not have been logged again.
  histograms().ExpectUniqueSample("Glic.ProfileEnablement.IsEnabled.Startup",
                                  false, 1);
}

TEST_F(GlicSplitButtonControllerTest, FromBrowserWindowInterface) {
  EXPECT_EQ(GlicSplitButtonController::From(browser_window_interface()),
            controller());
  EXPECT_EQ(GlicSplitButtonController::From(nullptr), nullptr);
}

TEST_F(GlicSplitButtonControllerTest, SetActorNudgeControllerForTesting) {
  auto mock_actor_nudge = std::make_unique<GlicActorNudgeController>(
      browser_window_interface(), controller());
  GlicActorNudgeController* raw_nudge = mock_actor_nudge.get();
  controller()->SetActorNudgeControllerForTesting(std::move(mock_actor_nudge));
  EXPECT_EQ(controller()->actor_nudge_controller(), raw_nudge);
}

TEST_F(GlicSplitButtonControllerTest, SplitButtonUiStateDefault) {
  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kNone);
}

TEST_F(GlicSplitButtonControllerTest, SplitButtonUiStateSingleActiveActivity) {
  auto* activity_manager = &glic_keyed_service()->activity_manager();
  ASSERT_TRUE(activity_manager);

  activity_manager->OnConversationActivityStateChanged(
      "conv-1", {.state = GlicActivityState::kInProgress});

  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kChevron);
}

TEST_F(GlicSplitButtonControllerTest,
       SplitButtonUiStateRevertsToChevronOnceViewed) {
  auto* activity_manager = &glic_keyed_service()->activity_manager();
  ASSERT_TRUE(activity_manager);

  activity_manager->OnConversationActivityStateChanged(
      "conv-1", {.state = GlicActivityState::kNeedsAttention});

  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kText);

  // Once viewed, the alert nudge reverts to the split chevron.
  activity_manager->MarkActivitySeen(
      GlicActivityKey{GlicActivityType::kConversationResponse, "conv-1"});
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kChevron);
}

TEST_F(GlicSplitButtonControllerTest, SplitButtonUiStateNeedsAttention) {
  auto* activity_manager = &glic_keyed_service()->activity_manager();
  ASSERT_TRUE(activity_manager);

  activity_manager->OnConversationActivityStateChanged(
      "conv-1", {.state = GlicActivityState::kNeedsAttention});

  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kText);
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_label,
            l10n_util::GetPluralStringFUTF16(
                IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL, 1));
}

TEST_F(GlicSplitButtonControllerTest, SplitButtonUiStateCompleteTasks) {
  base::test::ScopedFeatureList actor_ui_feature;
  actor_ui_feature.InitAndEnableFeature(features::kGlicActorUi);

  auto* actor_service = static_cast<actor::ActorKeyedServiceFake*>(
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile()));
  ASSERT_TRUE(actor_service);

  actor::TaskId task_id = actor_service->CreateTaskForTesting();
  actor_service->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);

  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kText);
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_label,
            l10n_util::GetPluralStringFUTF16(
                IDS_ACTOR_TASK_NUDGE_TASK_COMPLETE_LABEL, 1));
  EXPECT_TRUE(tab_strip_controller_delegate()->is_bubble_showing());
}

TEST_F(GlicSplitButtonControllerTest, SecondaryButtonClickTogglesBubble) {
  auto* activity_manager = &glic_keyed_service()->activity_manager();
  ASSERT_TRUE(activity_manager);

  activity_manager->OnConversationActivityStateChanged(
      "conv-1", {.state = GlicActivityState::kInProgress});
  EXPECT_FALSE(tab_strip_controller_delegate()->is_bubble_showing());

  controller()->OnSecondaryButtonClicked();
  EXPECT_TRUE(tab_strip_controller_delegate()->is_bubble_showing());

  controller()->OnSecondaryButtonClicked();
  EXPECT_FALSE(tab_strip_controller_delegate()->is_bubble_showing());

  // Re-open the bubble and verify that destroying the last activity transitions
  // secondary_action to kNone and closes the bubble automatically.
  controller()->OnSecondaryButtonClicked();
  EXPECT_TRUE(tab_strip_controller_delegate()->is_bubble_showing());
  activity_manager->OnConversationDestroyed("conv-1");
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kNone);
  EXPECT_FALSE(tab_strip_controller_delegate()->is_bubble_showing());
}

TEST_F(GlicSplitButtonControllerTest,
       SecondaryButtonClickLogsActorMetricOnlyForActorNudges) {
  base::UserActionTester user_actions;
  auto* activity_manager = &glic_keyed_service()->activity_manager();
  ASSERT_TRUE(activity_manager);

  // Clicking the split chevron for pure chat activity should not pollute Actor
  // global task indicator click actions.
  activity_manager->OnConversationActivityStateChanged(
      "conv-1", {.state = GlicActivityState::kInProgress});
  controller()->OnSecondaryButtonClicked();
  EXPECT_EQ(0, user_actions.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.Default.Click"));
  EXPECT_EQ(0, user_actions.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.NeedsAttention.Click"));

  // Close the bubble before transitioning to NeedsAttention.
  controller()->OnSecondaryButtonClicked();
  ASSERT_FALSE(tab_strip_controller_delegate()->is_bubble_showing());

  // Clicking to open the bubble during a NeedsAttention nudge logs the paired
  // Actor click action, while clicking again to close does not double-count.
  activity_manager->OnConversationActivityStateChanged(
      "conv-1", {.state = GlicActivityState::kNeedsAttention});
  controller()->OnSecondaryButtonClicked();
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.NeedsAttention.Click"));

  controller()->OnSecondaryButtonClicked();
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.NeedsAttention.Click"));
}

TEST_F(GlicSplitButtonControllerTest,
       PrimaryClickDoesNotToggleBubbleWhenNeedsAttention) {
  auto* activity_manager = &glic_keyed_service()->activity_manager();
  ASSERT_TRUE(activity_manager);

  activity_manager->OnConversationActivityStateChanged(
      "conv-1", {.state = GlicActivityState::kNeedsAttention});

  // When primary button is clicked, it invokes ToggleUI on GlicKeyedService,
  // never hijacking clicks to show the task bubble.
  EXPECT_CALL(*glic_keyed_service(),
              ToggleUI(testing::_, testing::_,
                       mojom::InvocationSource::kTopChromeButton))
      .Times(1);
  controller()->OnGlicButtonClicked();
}

TEST_F(GlicSplitButtonControllerTest,
       PrimaryClickWhenShowingNudgeUsesNudgeInvocationSource) {
  tab_strip_controller_delegate()->set_is_showing_nudge(true);
  EXPECT_CALL(*glic_keyed_service(),
              ToggleUI(browser_window_interface(), /*prevent_close=*/false,
                       mojom::InvocationSource::kNudge))
      .Times(1);
  controller()->OnGlicButtonClicked();

  tab_strip_controller_delegate()->set_is_showing_nudge(false);
  EXPECT_CALL(*glic_keyed_service(),
              ToggleUI(browser_window_interface(), /*prevent_close=*/false,
                       mojom::InvocationSource::kTopChromeButton))
      .Times(1);
  controller()->OnGlicButtonClicked();
}

TEST_F(GlicSplitButtonControllerTest,
       ActivityHubDisabledDoesNotUpdateSplitButtonUi) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(features::kGlicActivityHub);

  reset_controller();
  MockGlicSplitButtonDelegate legacy_tab_strip_delegate;
  MockGlicSplitButtonDelegate legacy_toolbar_delegate;

  GlicSplitButtonController legacy_controller(browser_window_interface(),
                                              glic_keyed_service());
  legacy_controller.SetHorizontalTabsDelegate(&legacy_tab_strip_delegate);
  legacy_controller.SetVerticalTabsDelegate(&legacy_toolbar_delegate);

  EXPECT_FALSE(legacy_tab_strip_delegate.last_ui_state().has_value());
  EXPECT_FALSE(legacy_toolbar_delegate.last_ui_state().has_value());
}

TEST_F(GlicSplitButtonControllerTest,
       SplitButtonUiStateResponseReadyShowsTextPill) {
  glic_keyed_service()->activity_manager().OnConversationActivityStateChanged(
      "conv-1", {.state = GlicActivityState::kCompleted});

  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kText);
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_label,
            l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_RESPONSE_READY));
}

TEST_F(GlicSplitButtonControllerTest,
       SplitButtonUiStateBackgroundInstancesShowChevron) {
  auto& activity_manager = glic_keyed_service()->activity_manager();

  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kNone);

  // When an idle registered conversation exists off the active tab, the chevron
  // appears.
  activity_manager.OnConversationActivityStateChanged(
      "conv-1",
      {.state = GlicActivityState::kIdle, .is_currently_visible = true});

  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kChevron);

  // Destroying the conversation hides the chevron again.
  activity_manager.OnConversationDestroyed("conv-1");

  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kNone);
}

TEST_F(GlicSplitButtonControllerTest,
       ActiveTabChangedClosesOrRefreshesOpenBubble) {
  auto& activity_manager = glic_keyed_service()->activity_manager();
  auto& coordinator = glic_keyed_service()->instance_coordinator_mock();

  tabs::MockTabInterface tab_1;
  tabs::MockTabInterface tab_2;
  tabs::MockTabInterface tab_3;

  // conv-1 is showing on tab_1 only; tab_2 and tab_3 have no panel showing.
  ON_CALL(coordinator, IsConversationPanelShowingForTab("conv-1", &tab_1))
      .WillByDefault(testing::Return(true));
  ON_CALL(coordinator, IsConversationPanelShowingForTab("conv-1", &tab_2))
      .WillByDefault(testing::Return(false));
  ON_CALL(coordinator, IsConversationPanelShowingForTab("conv-1", &tab_3))
      .WillByDefault(testing::Return(false));

  ON_CALL(*tab_list(), GetActiveTab()).WillByDefault(testing::Return(&tab_2));
  activity_manager.OnConversationActivityStateChanged(
      "conv-1",
      {.state = GlicActivityState::kIdle, .is_currently_visible = true});
  ASSERT_TRUE(tab_strip_controller_delegate()->last_ui_state().has_value());
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kChevron);

  // Open the activity list bubble on tab_2.
  controller()->OnSecondaryButtonClicked();
  EXPECT_TRUE(tab_strip_controller_delegate()->is_bubble_showing());
  EXPECT_EQ(tab_strip_controller_delegate()->show_bubble_count(), 1);

  // Switching to tab_3 keeps secondary_action as kChevron and refreshes the
  // open bubble for the new active tab.
  ON_CALL(*tab_list(), GetActiveTab()).WillByDefault(testing::Return(&tab_3));
  controller()->OnActiveTabChanged(*tab_list(), &tab_3);
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kChevron);
  EXPECT_TRUE(tab_strip_controller_delegate()->is_bubble_showing());
  EXPECT_EQ(tab_strip_controller_delegate()->show_bubble_count(), 2);

  // Switching to tab_1 (where conv-1's side panel is already showing) leaves no
  // secondary activities, hiding the chevron and closing the open bubble.
  ON_CALL(*tab_list(), GetActiveTab()).WillByDefault(testing::Return(&tab_1));
  controller()->OnActiveTabChanged(*tab_list(), &tab_1);
  EXPECT_EQ(tab_strip_controller_delegate()->last_ui_state()->secondary_action,
            GlicSplitButtonUiState::SecondaryAction::kNone);
  EXPECT_FALSE(tab_strip_controller_delegate()->is_bubble_showing());
}

}  // namespace glic
