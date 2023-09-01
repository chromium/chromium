// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/battery_notification.h"
#include "ash/test/test_widget_builder.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/welcome_tour/mock_welcome_tour_controller_observer.h"
#include "ash/user_education/welcome_tour/welcome_tour_accelerator_handler.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller_observer.h"
#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "ash/user_education/welcome_tour/welcome_tour_test_util.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_manager/user_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/types/event_type.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// Aliases.
using ::ash::welcome_tour_metrics::PreventedReason;
using ::base::test::RunOnceClosure;
using ::session_manager::SessionState;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsTrue;
using ::testing::Matches;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::StrictMock;
using ::user_education::HelpBubbleArrow;
using ::user_education::TutorialDescription;
using ::views::test::WidgetDestroyedWaiter;

using ContextMode = TutorialDescription::ContextMode;
using ElementSpecifier = TutorialDescription::ElementSpecifier;

// Actions ---------------------------------------------------------------------

// TODO(http://b/277094923): Try to promote to //base/test/gmock_move_support.h.
// Existing support is limited in that the gMock framework provides const-only
// access to `args` for all except the last action. This action lessens the
// effect of that limitation by supporting multiple moves at a time.
template <size_t... I, typename... T>
auto MoveArgs(T*... out) {
  return [out...](auto&&... args) {
    // Assigns the Ith-indexed value from `args` to `out` parameters by move.
    ([&]() { *out = std::move(std::get<I>(std::tie(args...))); }(), ...);
  };
}

// Matchers --------------------------------------------------------------------

MATCHER_P(ElementSpecifierEq, element_specifier, "") {
  return absl::visit(base::Overloaded{
                         [&](const ui::ElementIdentifier& element_id) {
                           return arg.element_id() == element_id &&
                                  arg.element_name().empty();
                         },
                         [&](const std::string& element_name) {
                           return arg.element_name() == element_name &&
                                  arg.element_id() == ui::ElementIdentifier();
                         },
                     },
                     element_specifier);
}

MATCHER_P6(BubbleStep,
           element_specifier,
           context_mode,
           help_bubble_id,
           body_text_id,
           arrow,
           has_next_button,
           "") {
  namespace util = user_education_util;
  const auto& ext_props = arg.extended_properties();
  return arg.step_type() == ui::InteractionSequence::StepType::kShown &&
         Matches(ElementSpecifierEq(element_specifier))(arg) &&
         arg.context_mode() == context_mode &&
         util::GetHelpBubbleId(ext_props) == help_bubble_id &&
         arg.body_text_id() == body_text_id && arg.arrow() == arrow &&
         arg.next_button_callback().is_null() != has_next_button &&
         util::GetHelpBubbleModalType(ext_props) == ui::MODAL_TYPE_SYSTEM &&
         &util::GetHelpBubbleBodyIcon(ext_props)->get() == &gfx::kNoneIcon;
}

MATCHER_P2(HiddenStep, element_specifier, context_mode, "") {
  return arg.step_type() == ui::InteractionSequence::StepType::kHidden &&
         Matches(ElementSpecifierEq(element_specifier))(arg) &&
         arg.context_mode() == context_mode;
}

MATCHER_P3(EventStep,
           element_specifier,
           context_mode,
           has_name_elements_callback,
           "") {
  return arg.step_type() == ui::InteractionSequence::StepType::kCustomEvent &&
         Matches(ElementSpecifierEq(element_specifier))(arg) &&
         arg.context_mode() == context_mode &&
         arg.name_elements_callback().is_null() != has_name_elements_callback;
}

MATCHER_P2(ShownStep, element_specifier, context_mode, "") {
  return arg.step_type() == ui::InteractionSequence::StepType::kShown &&
         Matches(ElementSpecifierEq(element_specifier))(arg) &&
         arg.context_mode() == context_mode;
}

// MockPretargetEventHandler ---------------------------------------------------

// A mock pre-target event handler to expose the received events.
class MockPretargetEventHandler : public ui::EventHandler {
 public:
  MockPretargetEventHandler() {
    Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
  }

  ~MockPretargetEventHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  MOCK_METHOD(void, OnKeyEvent, (ui::KeyEvent*), (override));
};

// MockView --------------------------------------------------------------------

// A mocked view to expose received events.
class MockView : public views::View {
 public:
  MockView() { SetFocusBehavior(views::View::FocusBehavior::ALWAYS); }

  // views::View:
  MOCK_METHOD(void, OnEvent, (ui::Event*), (override));
};

// Helpers ---------------------------------------------------------------------

// Adds a simple notification to the `message_center::MessageCenter` with the
// given `id`. If the optional argument `is_system_priority` is true, it will be
// set to the highest priority.
void AddNotification(const std::string& id, bool is_system_priority = false) {
  auto notification =
      SystemNotificationBuilder()
          .SetId(id)
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetDelegate(
              base::MakeRefCounted<message_center::NotificationDelegate>())
          .BuildPtr(false);
  if (is_system_priority) {
    notification->SetSystemPriority();
  }
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

// Checks the given expected notification counts against the current actual
// values in `message_center::MessageCenter`.
void ExpectNotificationCounts(size_t total_notifications,
                              size_t visible_notifications,
                              size_t popup_notifications) {
  auto* message_center = message_center::MessageCenter::Get();
  EXPECT_EQ(message_center->GetNotifications().size(), total_notifications);
  EXPECT_EQ(message_center->GetVisibleNotifications().size(),
            visible_notifications);
  EXPECT_EQ(message_center->GetPopupNotifications().size(),
            popup_notifications);
}

}  // namespace

// WelcomeTourControllerTest ---------------------------------------------------

// Base class for tests of the `WelcomeTourController`.
class WelcomeTourControllerTest : public UserEducationAshTestBase {
 public:
  WelcomeTourControllerTest() {
    // NOTE: The `WelcomeTourController` exists only when the Welcome Tour
    // feature is enabled. Controller existence is verified in test coverage
    // for the controller's owner.
    scoped_feature_list_.InitAndEnableFeature(features::kWelcomeTour);
  }

 protected:
  // UserEducationAshTestBase:
  void SetUp() override {
    UserEducationAshTestBase::SetUp();

    // Most tests of the `WelcomeTourController` are not concerned with user
    // eligibility, so provide a default implementation of `IsNewUser()` which
    // returns that the given user is "new" on invocation. "New"-ness is
    // required for the user to be eligible for the Welcome Tour.
    ON_CALL(*user_education_delegate(), IsNewUser)
        .WillByDefault(ReturnRefOfCopy(absl::make_optional(true)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `GetTutorialDescriptions()` returns expected values.
TEST_F(WelcomeTourControllerTest, GetTutorialDescriptions) {
  auto* welcome_tour_controller = WelcomeTourController::Get();
  ASSERT_TRUE(welcome_tour_controller);

  std::map<TutorialId, TutorialDescription> tutorial_descriptions_by_id =
      static_cast<UserEducationFeatureController*>(welcome_tour_controller)
          ->GetTutorialDescriptions();

  // TODO(http://b/275616974): Implement tutorial descriptions.
  EXPECT_EQ(tutorial_descriptions_by_id.size(), 1u);
  EXPECT_THAT(
      tutorial_descriptions_by_id,
      Contains(Pair(
          Eq(TutorialId::kWelcomeTourPrototype1),
          AllOf(
              Field(&TutorialDescription::complete_button_text_id,
                    Eq(IDS_ASH_WELCOME_TOUR_COMPLETE_BUTTON_TEXT)),
              Field(
                  &TutorialDescription::steps,
                  ElementsAre(
                      ShownStep(ElementSpecifier(kWelcomeTourDialogElementId),
                                ContextMode::kAny),
                      HiddenStep(ElementSpecifier(kWelcomeTourDialogElementId),
                                 ContextMode::kFromPreviousStep),
                      BubbleStep(ElementSpecifier(kShelfViewElementId),
                                 ContextMode::kInitial,
                                 HelpBubbleId::kWelcomeTourShelf,
                                 IDS_ASH_WELCOME_TOUR_SHELF_BUBBLE_BODY_TEXT,
                                 HelpBubbleArrow::kBottomCenter,
                                 /*has_next_button=*/true),
                      EventStep(ElementSpecifier(kShelfViewElementId),
                                ContextMode::kFromPreviousStep,
                                /*has_name_elements_callback=*/true),
                      BubbleStep(
                          ElementSpecifier(kUnifiedSystemTrayElementName),
                          ContextMode::kAny,
                          HelpBubbleId::kWelcomeTourStatusArea,
                          IDS_ASH_WELCOME_TOUR_STATUS_AREA_BUBBLE_BODY_TEXT,
                          HelpBubbleArrow::kBottomRight,
                          /*has_next_button=*/true),
                      EventStep(ElementSpecifier(kUnifiedSystemTrayElementName),
                                ContextMode::kFromPreviousStep,
                                /*has_name_elements_callback=*/true),
                      BubbleStep(
                          ElementSpecifier(kHomeButtonElementName),
                          ContextMode::kAny,
                          HelpBubbleId::kWelcomeTourHomeButton,
                          IDS_ASH_WELCOME_TOUR_HOME_BUTTON_BUBBLE_BODY_TEXT,
                          HelpBubbleArrow::kBottomLeft,
                          /*has_next_button=*/true),
                      BubbleStep(
                          ElementSpecifier(kSearchBoxViewElementId),
                          ContextMode::kAny,
                          HelpBubbleId::kWelcomeTourSearchBox,
                          IDS_ASH_WELCOME_TOUR_SEARCH_BOX_BUBBLE_BODY_TEXT,
                          HelpBubbleArrow::kTopCenter,
                          /*has_next_button=*/true),
                      EventStep(ElementSpecifier(kSearchBoxViewElementId),
                                ContextMode::kFromPreviousStep,
                                /*has_name_elements_callback=*/false),
                      BubbleStep(
                          ElementSpecifier(kSettingsAppElementId),
                          ContextMode::kFromPreviousStep,
                          HelpBubbleId::kWelcomeTourSettingsApp,
                          IDS_ASH_WELCOME_TOUR_SETTINGS_APP_BUBBLE_BODY_TEXT,
                          HelpBubbleArrow::kBottomLeft,
                          /*has_next_button=*/true),
                      EventStep(ElementSpecifier(kSettingsAppElementId),
                                ContextMode::kFromPreviousStep,
                                /*has_name_elements_callback=*/false),
                      BubbleStep(
                          ElementSpecifier(kExploreAppElementId),
                          ContextMode::kFromPreviousStep,
                          HelpBubbleId::kWelcomeTourExploreApp,
                          IDS_ASH_WELCOME_TOUR_EXPLORE_APP_BUBBLE_BODY_TEXT,
                          HelpBubbleArrow::kBottomLeft,
                          /*has_next_button=*/false)))))));
}

// Verifies that the Welcome Tour is started when the primary user session is
// first activated and then never again, as well as that start/end events are
// propagated to observers appropriately.
TEST_F(WelcomeTourControllerTest, StartsTourAndPropagatesEvents) {
  const auto primary_account_id = AccountId::FromUserEmail("primary@test");
  const auto secondary_account_id = AccountId::FromUserEmail("secondary@test");

  // Ensure controller exists.
  auto* const welcome_tour_controller = WelcomeTourController::Get();
  ASSERT_TRUE(welcome_tour_controller);

  // Ensure delegate exists and disallow any unexpected tutorial starts.
  auto* const user_education_delegate = this->user_education_delegate();
  ASSERT_TRUE(user_education_delegate);
  EXPECT_CALL(*user_education_delegate, StartTutorial).Times(0);

  // Observe the `WelcomeTourController` for start/end events.
  StrictMock<MockWelcomeTourControllerObserver> observer;
  base::ScopedObservation<WelcomeTourController, WelcomeTourControllerObserver>
      observation{&observer};
  observation.Observe(welcome_tour_controller);

  // Add a primary and secondary user session for the first time. This should
  // *not* trigger the Welcome Tour to start.
  auto* const session_controller_client = GetSessionControllerClient();
  session_controller_client->AddUserSession(
      primary_account_id.GetUserEmail(), user_manager::USER_TYPE_REGULAR,
      /*provide_pref_service=*/true, /*is_new_profile=*/true);
  session_controller_client->AddUserSession(
      secondary_account_id.GetUserEmail(), user_manager::USER_TYPE_REGULAR,
      /*provide_pref_service=*/true, /*is_new_profile=*/true);

  // Activate the primary user session. This *should* trigger the Welcome Tour
  // to start as well as notify observers. Note that completed/aborted
  // callbacks are cached for later verification.
  base::OnceClosure completed_callback;
  base::OnceClosure aborted_callback;
  EXPECT_CALL(
      *user_education_delegate,
      StartTutorial(Eq(primary_account_id),
                    Eq(TutorialId::kWelcomeTourPrototype1),
                    Eq(welcome_tour_controller->GetInitialElementContext()),
                    /*completed_callback=*/_,
                    /*aborted_callback=*/_))
      .WillOnce(MoveArgs<3, 4>(&completed_callback, &aborted_callback));
  EXPECT_CALL(observer, OnWelcomeTourStarted);
  session_controller_client->SetSessionState(SessionState::ACTIVE);
  Mock::VerifyAndClearExpectations(user_education_delegate);
  Mock::VerifyAndClearExpectations(&observer);

  // The Welcome Tour dialog is expected to be shown at the start of the tour.
  EXPECT_TRUE(WelcomeTourDialog::Get());

  // Disallow any unexpected tutorial starts.
  EXPECT_CALL(*user_education_delegate, StartTutorial).Times(0);

  // Switch to the secondary user session and back again. This should *not*
  // trigger the Welcome Tour to start.
  session_controller_client->SwitchActiveUser(secondary_account_id);
  session_controller_client->SwitchActiveUser(primary_account_id);

  // Deactivate and then reactivate the primary user session. This should *not*
  // trigger the Welcome Tour to start.
  session_controller_client->SetSessionState(SessionState::LOCKED);
  session_controller_client->SetSessionState(SessionState::ACTIVE);

  // Verify that the same event is propagated to observers regardless of whether
  // user education services in the browser indicate the tour was completed or
  // aborted. Only if the device is *not* in tablet mode should there be an
  // attempt to launch the Explore app.
  for (base::OnceClosure& ended_callback :
       {std::ref(completed_callback), std::ref(aborted_callback)}) {
    ASSERT_FALSE(ended_callback.is_null());
    EXPECT_CALL(observer, OnWelcomeTourEnded);
    EXPECT_CALL(*user_education_delegate,
                LaunchSystemWebAppAsync(
                    Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
                    Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())))
        .Times(TabletMode::IsInTabletMode() ? 0u : 1u);
    std::move(ended_callback).Run();
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(user_education_delegate);

    // Aborting the Welcome Tour should close the dialog.
    if (&ended_callback == &aborted_callback) {
      WidgetDestroyedWaiter(WelcomeTourDialog::Get()->GetWidget()).Wait();
      EXPECT_FALSE(WelcomeTourDialog::Get());
    }
  }
}

// Verifies that the Welcome Tour can be aborted via the dialog.
TEST_F(WelcomeTourControllerTest, AbortsTourAndPropagatesEvents) {
  const auto primary_account_id = AccountId::FromUserEmail("primary@test");

  // Expect the Welcome Tour to start when logging in the primary user. Note
  // that the `aborted_callback` is cached.
  base::OnceClosure aborted_callback;
  EXPECT_CALL(*user_education_delegate(),
              StartTutorial(Eq(primary_account_id),
                            Eq(TutorialId::kWelcomeTourPrototype1), _, _, _))
      .WillOnce(MoveArg<4>(&aborted_callback));

  // Start the Welcome Tour by logging in the primary user for the first time.
  SimulateNewUserFirstLogin(primary_account_id.GetUserEmail());

  // Observe the `WelcomeTourController` for end events.
  StrictMock<MockWelcomeTourControllerObserver> observer;
  base::ScopedObservation<WelcomeTourController, WelcomeTourControllerObserver>
      observation{&observer};
  observation.Observe(WelcomeTourController::Get());

  // Satisfy `ended_future` when an end event is received.
  base::test::TestFuture<void> ended_future;
  EXPECT_CALL(observer, OnWelcomeTourEnded)
      .WillOnce(RunOnceClosure(ended_future.GetCallback()));

  // Expect the Welcome Tour to be aborted when clicking the `cancel_button`.
  // Fulfill the request to abort the tour by running the `aborted_callback`.
  EXPECT_CALL(*user_education_delegate(),
              AbortTutorial(Eq(primary_account_id),
                            Eq(TutorialId::kWelcomeTourPrototype1)))
      .WillOnce(RunOnceClosure(std::move(aborted_callback)));

  // Expect an attempt to launch the Explore app when the tour is aborted.
  EXPECT_CALL(*user_education_delegate(),
              LaunchSystemWebAppAsync(
                  Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
                  Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())));

  // Click the `cancel_button` and verify the Welcome Tour is ended.
  const views::View* const cancel_button = GetDialogCancelButton();
  ASSERT_TRUE(cancel_button);
  LeftClickOn(cancel_button);
  EXPECT_TRUE(ended_future.Wait());
}

// Verifies the Welcome Tour to be aborted if ChromeVox is enabled during tour.
TEST_F(WelcomeTourControllerTest, AbortTourIfChromeVoxEnabledDuringTour) {
  // Start the Welcome Tour by logging in the primary user for the first time.
  const auto primary_account_id = AccountId::FromUserEmail("primary@test");
  SimulateNewUserFirstLogin(primary_account_id.GetUserEmail());

  // Observe the `WelcomeTourController` for end events.
  StrictMock<MockWelcomeTourControllerObserver> observer;
  base::ScopedObservation<WelcomeTourController, WelcomeTourControllerObserver>
      observation{&observer};
  observation.Observe(WelcomeTourController::Get());

  // Satisfy `ended_future` when an end event is received.
  base::test::TestFuture<void> ended_future;
  EXPECT_CALL(observer, OnWelcomeTourEnded)
      .WillOnce(RunOnceClosure(ended_future.GetCallback()));

  // Expect the Welcome Tour to be aborted when enabling ChromeVox during tour.
  EXPECT_CALL(*user_education_delegate(),
              AbortTutorial(Eq(primary_account_id),
                            Eq(TutorialId::kWelcomeTourPrototype1)));

  // Expect an attempt to launch the Explore app when the tour is aborted.
  EXPECT_CALL(*user_education_delegate(),
              LaunchSystemWebAppAsync(
                  Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
                  Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())));

  auto* const a11y_controller = Shell::Get()->accessibility_controller();
  a11y_controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  Mock::VerifyAndClearExpectations(user_education_delegate());
  EXPECT_TRUE(a11y_controller->spoken_feedback().enabled());
}

// Checks that the Welcome Tour should NOT start if ChromeVox is enabled.
TEST_F(WelcomeTourControllerTest, PreventTourFromStartingIfChromeVoxEnabled) {
  const auto primary_account_id = AccountId::FromUserEmail("primary@test");

  TestSessionControllerClient* const session = GetSessionControllerClient();
  session->AddUserSession(
      primary_account_id.GetUserEmail(), user_manager::USER_TYPE_REGULAR,
      /*provide_pref_service=*/true, /*is_new_profile=*/true);
  session->SwitchActiveUser(primary_account_id);

  // Enable the spoken feedback after the pref service is ready and before the
  // session becomes active.
  auto* const a11y_controller = Shell::Get()->accessibility_controller();
  a11y_controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(a11y_controller->spoken_feedback().enabled());

  // Start the Welcome Tour by activating the user session. Expect that the
  // Welcome Tour is NOT started but that an attempt is made to launch the
  // Explore app.
  EXPECT_CALL(*user_education_delegate(), StartTutorial).Times(0);
  EXPECT_CALL(*user_education_delegate(),
              LaunchSystemWebAppAsync(
                  Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
                  Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())));
  session->SetSessionState(SessionState::ACTIVE);
  Mock::VerifyAndClearExpectations(user_education_delegate());
}

// WelcomeTourControllerCounterfactualTest -------------------------------------

// Base class for tests of the `WelcomeTourController` which are concerned with
// the behavior of counterfactual experiment arms, parameterized by whether the
// Welcome Tour feature is enabled counterfactually.
class WelcomeTourControllerCounterfactualTest
    : public WelcomeTourControllerTest,
      public ::testing::WithParamInterface<
          /*is_counterfactual=*/absl::optional<bool>> {
 public:
  WelcomeTourControllerCounterfactualTest() {
    if (const auto& is_counterfactual = IsCounterfactual()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kWelcomeTour,
          {{"is-counterfactual", *is_counterfactual ? "true" : "false"}});
    }
  }

  // Returns whether the Welcome Tour is enabled counterfactually as part of an
  // experiment arm given test parameterization.
  const absl::optional<bool>& IsCounterfactual() const { return GetParam(); }

 private:
  // Used to conditionally enable the Welcome Tour counterfactually as part of
  // an experiment arm given test parameterization.
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WelcomeTourControllerCounterfactualTest,
                         /*is_counterfactual=*/
                         ::testing::Values(absl::make_optional(true),
                                           absl::make_optional(false),
                                           absl::nullopt));

// Tests -----------------------------------------------------------------------

// Verifies that the Welcome Tour is prevented from running if enabled
// counterfactually as part of an experiment arm and that an attempt is made to
// launch the Explore app when that occurs.
TEST_P(WelcomeTourControllerCounterfactualTest,
       PreventsWelcomeTourForCounterfactualArms) {
  const auto primary_account_id = AccountId::FromUserEmail("primary@test");

  // Set expectations for whether the Welcome Tour will run.
  EXPECT_CALL(*user_education_delegate(),
              StartTutorial(Eq(primary_account_id),
                            Eq(TutorialId::kWelcomeTourPrototype1), _, _, _))
      .Times(IsCounterfactual().value_or(false) ? 0u : 1u);

  // Set expectations for whether the Explore app will launch.
  EXPECT_CALL(*user_education_delegate(),
              LaunchSystemWebAppAsync(
                  Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
                  Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())))
      .Times(IsCounterfactual().value_or(false) ? 1u : 0u);

  // Login the primary user for the first time and verify expectations.
  SimulateNewUserFirstLogin(primary_account_id.GetUserEmail());
  Mock::VerifyAndClearExpectations(user_education_delegate());
}

// WelcomeTourControllerUserEligibilityTest ------------------------------------

// Base class for tests of the `WelcomeTourController` which are concerned with
// user eligibility, parameterized by:
// (a) whether to force user eligibility via feature flag
// (b) whether the user should be considered "new" cross-device
// (c) whether the user should be considered "new" locally
// (d) whether the user is managed
// (e) the user type.
class WelcomeTourControllerUserEligibilityTest
    : public WelcomeTourControllerTest,
      public ::testing::WithParamInterface<std::tuple<
          /*force_user_eligibility=*/bool,
          /*is_new_user_cross_device=*/absl::optional<bool>,
          /*is_new_user_locally=*/bool,
          /*is_managed_user=*/bool,
          user_manager::UserType>> {
 public:
  WelcomeTourControllerUserEligibilityTest() {
    // Conditionally force user eligibility based on test parameterization.
    scoped_feature_list_.InitWithFeatureState(
        features::kWelcomeTourForceUserEligibility, ForceUserEligibility());
  }

  // Returns the account ID of the primary user for whom to test eligibility.
  const AccountId& primary_account_id() const { return primary_account_id_; }

  // Returns whether user eligibility is forced based on test parameterization.
  bool ForceUserEligibility() const { return std::get<0>(GetParam()); }

  // Returns the user type based on test parameterization.
  user_manager::UserType GetUserType() const { return std::get<4>(GetParam()); }

  // Returns whether the user is managed based on test parameterization.
  bool IsManagedUser() const { return std::get<3>(GetParam()); }

  // Returns whether the user should be considered "new" cross-device based on
  // test parameterization.
  const absl::optional<bool>& IsNewUserCrossDevice() const {
    return std::get<1>(GetParam());
  }

  // Returns whether the user should be considered "new" locally based on test
  // parameterization.
  bool IsNewUserLocally() const { return std::get<2>(GetParam()); }

 private:
  // WelcomeTourControllerTest:
  void SetUp() override {
    WelcomeTourControllerTest::SetUp();

    // Provide an implementation of `IsNewUser()` which returns whether a given
    // user should be considered "new" cross-device based on test
    // parameterization.
    ON_CALL(*user_education_delegate(), IsNewUser)
        .WillByDefault(ReturnRefOfCopy(IsNewUserCrossDevice()));

    // Add a user based on test parameterization.
    TestSessionControllerClient* const session = GetSessionControllerClient();
    session->AddUserSession(primary_account_id_.GetUserEmail(), GetUserType(),
                            /*provide_pref_service=*/true,
                            /*is_new_profile=*/IsNewUserLocally(),
                            /*given_name=*/std::string(), IsManagedUser());
    session->SwitchActiveUser(primary_account_id_);
  }

  // Used to conditionally force user eligibility based on test
  // parameterization.
  base::test::ScopedFeatureList scoped_feature_list_;

  // The account ID of the primary user for whom to test eligibility.
  const AccountId primary_account_id_ =
      AccountId::FromUserEmail("primary@test");
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WelcomeTourControllerUserEligibilityTest,
    ::testing::Combine(
        /*force_user_eligibility=*/::testing::Bool(),
        /*is_new_user_cross_device=*/
        ::testing::Values(absl::make_optional(true),
                          absl::make_optional(false),
                          absl::nullopt),
        /*is_new_user_locally=*/::testing::Bool(),
        /*is_managed_user=*/::testing::Bool(),
        ::testing::Values(user_manager::UserType::USER_TYPE_ARC_KIOSK_APP,
                          user_manager::UserType::USER_TYPE_CHILD,
                          user_manager::UserType::USER_TYPE_GUEST,
                          user_manager::UserType::USER_TYPE_KIOSK_APP,
                          user_manager::UserType::USER_TYPE_PUBLIC_ACCOUNT,
                          user_manager::UserType::USER_TYPE_REGULAR,
                          user_manager::UserType::USER_TYPE_WEB_KIOSK_APP)));

// Tests -----------------------------------------------------------------------

// Verifies that user eligibility for the Welcome Tour is enforced as expected.
TEST_P(WelcomeTourControllerUserEligibilityTest, EnforcesUserEligibility) {
  // A user is eligible for the Welcome Tour if and only if:
  // (a) user eligibility is being explicitly forced, or
  // (b) the user satisfies the following conditions:
  //     (1) known to be "new" cross-device on session activation, and
  //     (2) known to be "new" locally, and
  //     (3) not a managed user, and
  //     (4) a regular user.
  const bool is_user_eligibility_expected =
      ForceUserEligibility() ||
      (IsNewUserCrossDevice().value_or(false) && IsNewUserLocally() &&
       !IsManagedUser() && GetUserType() == user_manager::USER_TYPE_REGULAR);

  // Set expectations for whether the Welcome Tour will run.
  EXPECT_CALL(*user_education_delegate(),
              StartTutorial(Eq(primary_account_id()),
                            Eq(TutorialId::kWelcomeTourPrototype1), _, _, _))
      .Times(is_user_eligibility_expected ? 1u : 0u);

  // If the Welcome Tour is run, we delay attempts to launch the Explore app
  // until the tour is completed or aborted. If the Welcome Tour is not run, the
  // user is not new so there should similarly be no attempt to launch Explore.
  EXPECT_CALL(*user_education_delegate(),
              LaunchSystemWebAppAsync(
                  Eq(primary_account_id()), Eq(ash::SystemWebAppType::HELP),
                  Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())))
      .Times(0);

  base::HistogramTester histogram_tester;

  // Activate the user session and verify expectations.
  GetSessionControllerClient()->SetSessionState(SessionState::ACTIVE);
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // Verify histograms.
  // NOTE: Order is important. For users not going through OOBE, it is expected
  // that cross-device newness be unavailable. To ensure that the most specific
  // prevention reason is emitted to histograms, cross-device newness checks
  // should be last.
  std::vector<base::Bucket> buckets;
  if (!ForceUserEligibility()) {
    if (GetUserType() != user_manager::USER_TYPE_REGULAR) {
      buckets.emplace_back(PreventedReason::kUserTypeNotRegular, 1);
    } else if (IsManagedUser()) {
      buckets.emplace_back(PreventedReason::kManagedAccount, 1);
    } else if (!IsNewUserLocally()) {
      buckets.emplace_back(PreventedReason::kUserNotNewLocally, 1);
    } else if (!IsNewUserCrossDevice().has_value()) {
      buckets.emplace_back(PreventedReason::kUserNewnessNotAvailable, 1);
    } else if (!IsNewUserCrossDevice().value()) {
      buckets.emplace_back(PreventedReason::kUserNotNewCrossDevice, 1);
    }
  }
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Ash.WelcomeTour.Prevented.Reason"),
      base::BucketsAreArray(buckets));
}

// WelcomeTourControllerRunTest ------------------------------------------------

// Base class for tests of the `WelcomeTourController` that run the Welcome
// Tour in order to assert expectations before, during, and/or after run time.
class WelcomeTourControllerRunTest : public WelcomeTourControllerTest {
 public:
  WelcomeTourControllerRunTest() {
    // Enable the `AnchoredNudgeManager` as it has an easier to use syntax than
    // the `SystemNudgeController` which is on its way out the door.
    scoped_feature_list_.InitAndEnableFeature(features::kSystemNudgeV2);
  }

  // Runs the Welcome Tour, invoking the specified `in_progress_callback` just
  // after the Welcome Tour has started. Note that this method will not return
  // until the Welcome Tour has ended.
  void Run(base::OnceClosure in_progress_callback) {
    // Ensure `controller` exists.
    auto* const controller = WelcomeTourController::Get();
    ASSERT_TRUE(controller);

    // Ensure `delegate` exists.
    auto* const delegate = user_education_delegate();
    ASSERT_TRUE(delegate);

    // Observe the `controller` for Welcome Tour start/end events.
    StrictMock<MockWelcomeTourControllerObserver> observer;
    base::ScopedObservation<WelcomeTourController,
                            WelcomeTourControllerObserver>
        observation{&observer};
    observation.Observe(controller);

    // When the Welcome Tour starts/ends, signal the appropriate future.
    base::test::TestFuture<void> started_future;
    base::test::TestFuture<void> ended_future;
    EXPECT_CALL(observer, OnWelcomeTourStarted)
        .WillOnce(RunOnceClosure(started_future.GetCallback()));
    EXPECT_CALL(observer, OnWelcomeTourEnded)
        .WillOnce(RunOnceClosure(ended_future.GetCallback()));

    const auto primary_account_id = AccountId::FromUserEmail("primary@test");

    // When the Welcome Tour tutorial is started, cache the callback to invoke
    // to complete the tutorial.
    base::OnceClosure completed_callback;
    EXPECT_CALL(*delegate,
                StartTutorial(Eq(primary_account_id),
                              Eq(TutorialId::kWelcomeTourPrototype1), _, _, _))
        .WillOnce(MoveArg<3>(&completed_callback));

    // Simulate login of the primary user for the first time. Note that this
    // should trigger the Welcome Tour to start automatically.
    SimulateNewUserFirstLogin(primary_account_id.GetUserEmail());
    EXPECT_TRUE(started_future.Wait());

    // Invoke the `in_progress_callback` so that tests can assert expectations
    // while the Welcome Tour is in progress.
    std::move(in_progress_callback).Run();

    // When the tour is completed, expect an attempt to launch the Explore app.
    EXPECT_CALL(
        *user_education_delegate(),
        LaunchSystemWebAppAsync(
            Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
            Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())));

    // Click `accept_button` to close the Welcome Tour dialog.
    const views::View* const accept_button = GetDialogAcceptButton();
    ASSERT_TRUE(accept_button);
    LeftClickOn(accept_button);

    // Complete the tutorial by invoking the cached callback.
    std::move(completed_callback).Run();
    EXPECT_TRUE(ended_future.Wait());
    Mock::VerifyAndClearExpectations(user_education_delegate());
  }

 private:
  // Used to enable the `AnchoredNudgeManager`.
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

TEST_F(WelcomeTourControllerRunTest, BlockInteractionsWithIrrelevantWindow) {
  // Create a random widget to interact with.
  std::unique_ptr<views::Widget> widget =
      TestWidgetBuilder()
          .SetBounds(gfx::Rect(100, 100))
          .SetParent(Shell::GetPrimaryRootWindow()->GetChildById(
              kShellWindowId_LockScreenContainer))
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildOwnsNativeWidget();
  MockView* const contents_view =
      widget->SetContentsView(std::make_unique<MockView>());
  widget->GetFocusManager()->SetFocusedView(contents_view);

  // `contents_view` should be interactive before the Welcome Tour.
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_ENTERED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_PRESSED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_EXITED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_KEY_PRESSED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_KEY_RELEASED))));
  LeftClickOn(contents_view);
  PressAndReleaseKey(ui::VKEY_A);

  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting([&]() {
        // During the Welcome Tour, `contents_view` should NOT be interactive.
        EXPECT_CALL(*contents_view, OnEvent).Times(0);
        LeftClickOn(contents_view);
        PressAndReleaseKey(ui::VKEY_A);
      })));

  // Perform the same set of actions after the Welcome Tour. `contents_view`
  // should be interactive.
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_ENTERED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_PRESSED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_EXITED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_KEY_PRESSED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_KEY_RELEASED))));
  LeftClickOn(contents_view);
  PressAndReleaseKey(ui::VKEY_A);
}

// Verifies that notifications are blocked during and only during the Welcome
// Tour.
TEST_F(WelcomeTourControllerRunTest, NotificationBlocking) {
  constexpr char kSystemPriorityId[] = "system";
  auto* message_center = message_center::MessageCenter::Get();

  // Case: Before Welcome Tour.
  // Most notifications should be muted, with few exceptions, because no user is
  // logged in.
  {
    SCOPED_TRACE("Initial state");
    ExpectNotificationCounts(/*total_notifications=*/0u,
                             /*visible_notifications=*/0u,
                             /*popup_notifications=*/0u);
  }

  {
    SCOPED_TRACE("Simple notification before login");
    AddNotification("test1");
    ExpectNotificationCounts(
        /*total_notifications=*/1u,
        /*visible_notifications=*/0u,
        /*popup_notifications=*/0u);
  }

  {
    SCOPED_TRACE("Battery notification before login");

    // Use the id from `BatteryNotification` to confirm that those notifications
    // are not muted, as battery notifications show even during OOBE. Note that
    // it does not have system priority; the OOBE exception is based on its id.
    AddNotification(BatteryNotification::kNotificationId);
    ExpectNotificationCounts(
        /*total_notifications=*/2u,
        /*visible_notifications=*/1u,
        /*popup_notifications=*/1u);
  }

  // Case: During Welcome Tour.
  // Notification blocking should hide notifications, including hiding any
  // existing popups.
  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting([&]() {
        {
          SCOPED_TRACE("Beginning of Welcome Tour");
          ExpectNotificationCounts(
              /*total_notifications=*/2u,
              /*visible_notifications=*/0u,
              /*popup_notifications=*/0u);
        }

        {
          SCOPED_TRACE("Simple notification during Welcome Tour");
          AddNotification("test2");
          ExpectNotificationCounts(
              /*total_notifications=*/3u,
              /*visible_notifications=*/0u,
              /*popup_notifications=*/0u);
        }

        {
          SCOPED_TRACE("System priority notification during Welcome Tour");

          // System priority notifications should show a popup after the tour is
          // over.
          AddNotification(kSystemPriorityId, /*is_system_priority=*/true);
          ExpectNotificationCounts(
              /*total_notifications=*/4u,
              /*visible_notifications=*/0u,
              /*popup_notifications=*/0u);
        }
      })));

  // Case: After Welcome Tour.
  // All notifications should now be shown in the list. Notifications that
  // happened before or during the tour should not have popups, but new
  // notifications should.
  {
    SCOPED_TRACE("Just after Welcome Tour");
    ExpectNotificationCounts(
        /*total_notifications=*/4u,
        /*visible_notifications=*/4u,
        /*popup_notifications=*/1u);
  }

  // Confirm that the one popup showing is the one with system priority.
  EXPECT_THAT(message_center->GetPopupNotifications(),
              ElementsAre(Property(&message_center::Notification::id,
                                   Eq(kSystemPriorityId))));

  {
    SCOPED_TRACE("Notification after Welcome Tour");
    AddNotification("test3");
    ExpectNotificationCounts(
        /*total_notifications=*/5u,
        /*visible_notifications=*/5u,
        /*popup_notifications=*/2u);
  }
}

// Verifies that nudges are suppressed while the Welcome Tour is in progress.
TEST_F(WelcomeTourControllerRunTest, NudgePause) {
  static constexpr char kNudgeId[] = "nudge_id";

  // Verify nudge manager exists.
  auto* const nudge_manager = AnchoredNudgeManager::Get();
  ASSERT_TRUE(nudge_manager);

  // Cache helper to check if a nudge matching `kNudgeId` is showing.
  auto is_showing_nudge = [&]() {
    return nudge_manager->IsNudgeShown(kNudgeId);
  };

  // Cache helper to show a toast matching `kNudgeId`.
  auto show_nudge = [&]() {
    AnchoredNudgeData nudge(kNudgeId, NudgeCatalogName::kMaxValue, u"text");
    nudge_manager->Show(nudge);
  };

  // Case: Before Welcome Tour.
  EXPECT_FALSE(is_showing_nudge());
  show_nudge();
  EXPECT_TRUE(is_showing_nudge());

  // Case: During Welcome Tour.
  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting([&]() {
        EXPECT_FALSE(is_showing_nudge());
        show_nudge();
        EXPECT_FALSE(is_showing_nudge());
      })));

  // Case: After Welcome Tour.
  EXPECT_FALSE(is_showing_nudge());
  show_nudge();
  EXPECT_TRUE(is_showing_nudge());
}

// Verifies that scrims are added to all root windows only while the Welcome
// Tour is in progress.
TEST_F(WelcomeTourControllerRunTest, Scrim) {
  // Case: Before Welcome Tour.
  ExpectScrimsOnAllRootWindows(false);

  // Case: During Welcome Tour.
  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting(
          [&]() { ExpectScrimsOnAllRootWindows(true); })));

  // Case: After Welcome Tour.
  ExpectScrimsOnAllRootWindows(false);
}

// Verifies that toasts are suppressed while the Welcome Tour is in progress.
TEST_F(WelcomeTourControllerRunTest, ToastPause) {
  static constexpr char kToastId[] = "toast_id";

  // Verify toast manager exists.
  auto* const toast_manager = ToastManager::Get();
  ASSERT_TRUE(toast_manager);

  // Cache helper to check if a toast matching `kToastId` is showing.
  auto is_showing_toast = [&]() { return toast_manager->IsRunning(kToastId); };

  // Cache helper to show a toast matching `kToastId`.
  auto show_toast = [&]() {
    toast_manager->Show(ToastData(kToastId, ToastCatalogName::kMaxValue,
                                  u"text", ToastData::kInfiniteDuration,
                                  /*visible_on_lock_screen=*/true));
  };

  // Case: Before Welcome Tour.
  EXPECT_FALSE(is_showing_toast());
  show_toast();
  EXPECT_TRUE(is_showing_toast());

  // Case: During Welcome Tour.
  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting([&]() {
        EXPECT_FALSE(is_showing_toast());
        show_toast();
        EXPECT_FALSE(is_showing_toast());
      })));

  // Case: After Welcome Tour.
  EXPECT_FALSE(is_showing_toast());
  show_toast();
  EXPECT_TRUE(is_showing_toast());
}

// Verifies that windows are minimized iff the Welcome Tour is in progress.
TEST_F(WelcomeTourControllerRunTest, WindowMinimizer) {
  auto window_1 = CreateAppWindow();

  // Case: Before Welcome Tour.
  EXPECT_THAT(window_1, Minimized(Eq(false)));

  // Case: During Welcome Tour.
  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(WaitUntilMinimized(window_1.get()));
        auto window_2 = CreateAppWindow();
        EXPECT_TRUE(WaitUntilMinimized(window_2.get()));
      })));

  // Case: After Welcome Tour.
  EXPECT_THAT(window_1, Minimized(Eq(true)));
  auto window_3 = CreateAppWindow();
  EXPECT_THAT(window_3, Minimized(Eq(false)));
}

// Verifies accelerator actions before/during/after the Welcome Tour.
class WelcomeTourAcceleratorHandlerRunTest
    : public WelcomeTourControllerRunTest {
 public:
  // WelcomeTourControllerRunTest:
  void SetUp() override {
    WelcomeTourControllerRunTest::SetUp();

    // Create a mock pre-target event handler that always consumes key events.
    mock_pretarget_event_handler_ =
        std::make_unique<MockPretargetEventHandler>();
    ON_CALL(*mock_pretarget_event_handler_, OnKeyEvent)
        .WillByDefault(
            [](ui::KeyEvent* key_event) { key_event->StopPropagation(); });
  }

  void TearDown() override {
    mock_pretarget_event_handler_.reset();
    WelcomeTourControllerRunTest::TearDown();
  }

  // Performs the specified accelerator action. Then checks the key events
  // received as expected.
  void PerformActionAndCheckKeyEvents(AcceleratorAction action, bool received) {
    // Get the accelerators corresponding to `action`.
    const std::vector<ui::Accelerator>& accelerators =
        Shell::Get()->ash_accelerator_configuration()->GetAcceleratorsForAction(
            action);
    ASSERT_FALSE(accelerators.empty());

    for (const ui::Accelerator& accelerator : accelerators) {
      // If `received` is true, then `accelerator` should be received;
      // otherwise, `accelerator` should NOT be received.
      EXPECT_CALL(
          *mock_pretarget_event_handler_,
          OnKeyEvent(AllOf(
              Property(&ui::KeyEvent::type,
                       Eq(accelerator.key_state() ==
                                  ui::Accelerator::KeyState::PRESSED
                              ? ui::ET_KEY_PRESSED
                              : ui::ET_KEY_RELEASED)),
              Property(&ui::KeyEvent::key_code, Eq(accelerator.key_code())))))
          .Times(received ? 1u : 0u);

      // The key event that does NOT trigger `action` should be received.
      EXPECT_CALL(
          *mock_pretarget_event_handler_,
          OnKeyEvent(AllOf(
              Property(&ui::KeyEvent::type,
                       Eq(accelerator.key_state() ==
                                  ui::Accelerator::KeyState::PRESSED
                              ? ui::ET_KEY_RELEASED
                              : ui::ET_KEY_PRESSED)),
              Property(&ui::KeyEvent::key_code, Eq(accelerator.key_code())))));

      // Press and release `accelerator`.
      PressAndReleaseKey(accelerator.key_code(), accelerator.modifiers());
      Mock::VerifyAndClearExpectations(mock_pretarget_event_handler_.get());
    }
  }

  void VerifyActionsInAllowedList() {
    for (auto allowed_action : WelcomeTourAcceleratorHandler::kAllowedActions) {
      PerformActionAndCheckKeyEvents(allowed_action.action,
                                     /*received=*/true);
    }
  }

  std::unique_ptr<MockPretargetEventHandler> mock_pretarget_event_handler_;
};

// Tests -----------------------------------------------------------------------

// Verifies that the key events that trigger the allowed accelerator actions are
// received during the Welcome Tour.
TEST_F(WelcomeTourAcceleratorHandlerRunTest, AllowActionsInAllowedList) {
  // Verify that before the Welcome Tour, the key events for the actions in the
  // allowed list are received by the mock event handler.
  VerifyActionsInAllowedList();

  // Verify that during the Welcome Tour, the key events for these actions are
  // received by the mock event handler.
  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting(
          [&]() { VerifyActionsInAllowedList(); })));

  // Verify that after the Welcome Tour, the key events for these actions are
  // received by the mock event handler.
  VerifyActionsInAllowedList();
}

// Verifies that the accelerator actions NOT in the allowed list should be
// blocked during the Welcome Tour.
TEST_F(WelcomeTourAcceleratorHandlerRunTest, BlockActionsNotInAllowedList) {
  // Verify that before the Welcome Tour, the key events for the actions that
  // are NOT in the allowed list are received by the mock event handler.
  PerformActionAndCheckKeyEvents(AcceleratorAction::kTakePartialScreenshot,
                                 /*received=*/true);

  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting([&]() {
        // During the Welcome Tour, the key events that trigger these actions
        // should NOT be received by the mock event handler.
        PerformActionAndCheckKeyEvents(
            AcceleratorAction::kTakePartialScreenshot,
            /*received=*/false);
      })));

  // Verify that after the Welcome Tour, these key events are received by
  // the mock event handler.
  PerformActionAndCheckKeyEvents(AcceleratorAction::kTakePartialScreenshot,
                                 /*received=*/true);
}

// Verifies the accelerator actions that abort the Welcome Tour when performed.
TEST_F(WelcomeTourAcceleratorHandlerRunTest, CheckActionsThatAbortTour) {
  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting([&]() {
        for (const auto& allowed_action :
             WelcomeTourAcceleratorHandler::kAllowedActions) {
          if (!allowed_action.aborts_tour) {
            // Skip `allowed_action` if it does not need to abort the tour.
            continue;
          }

          base::test::TestFuture<void> aborted_future;
          EXPECT_CALL(*user_education_delegate(), AbortTutorial)
              .WillOnce(RunOnceClosure(aborted_future.GetCallback()));

          // During the Welcome Tour, the key events for `action` should be
          // received by the mock event handler.
          PerformActionAndCheckKeyEvents(allowed_action.action,
                                         /*received=*/true);

          // The delegate API that aborts the Welcome Tour should be called.
          EXPECT_TRUE(aborted_future.Wait());
          Mock::VerifyAndClearExpectations(user_education_delegate());
        }
      })));
}

// WelcomeTourControllerTabletTest ---------------------------------------------

// Base class for tests of the `WelcomeTourController` that verify the tour does
// not start/will abort in tablet mode.
class WelcomeTourControllerTabletTest : public WelcomeTourControllerTest {
 public:
  WelcomeTourController* controller() { return WelcomeTourController::Get(); }
  StrictMock<MockWelcomeTourControllerObserver>* observer() {
    return &observer_;
  }

  // WelcomeTourControllerTest:
  void SetUp() override {
    WelcomeTourControllerTest::SetUp();

    // Observe the `WelcomeTourController` for start/end events.
    observation_.Observe(controller());
  }

  void TearDown() override {
    observation_.Reset();

    WelcomeTourControllerTest::TearDown();
  }

 private:
  StrictMock<MockWelcomeTourControllerObserver> observer_;
  base::ScopedObservation<WelcomeTourController, WelcomeTourControllerObserver>
      observation_{&observer_};
};

// Tests -----------------------------------------------------------------------

// Verifies that the Welcome Tour will not start when in tablet mode.
TEST_F(WelcomeTourControllerTabletTest, DoesNotStart) {
  const auto primary_account_id = AccountId::FromUserEmail("primary@test");

  // Force tablet mode on.
  TabletMode::Get()->SetEnabledForTest(true);

  // Activate the primary user session for the first time. Since tablet mode is
  // enabled, the Welcome Tour should not start, the dialog should not show,
  // there should be no attempt to launch the Explore app, and no start or end
  // calls should be made.
  EXPECT_CALL(*user_education_delegate(), StartTutorial).Times(0);
  EXPECT_CALL(*user_education_delegate(),
              LaunchSystemWebAppAsync(
                  Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
                  Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())))
      .Times(0);
  SimulateNewUserFirstLogin(primary_account_id.GetUserEmail());
  EXPECT_FALSE(WelcomeTourDialog::Get());
  Mock::VerifyAndClearExpectations(user_education_delegate());
  Mock::VerifyAndClearExpectations(observer());
}

// Verifies that the tour will abort if we enter tablet mode.
TEST_F(WelcomeTourControllerTabletTest, TriggersAbort) {
  const auto primary_account_id = AccountId::FromUserEmail("primary@test");

  // Activate the user session for the first time to trigger the Welcome Tour to
  // start, as well as notify observers. Note that the aborted callback is
  // cached for later verification.
  base::OnceClosure aborted_callback;
  EXPECT_CALL(*user_education_delegate(),
              StartTutorial(Eq(primary_account_id),
                            Eq(TutorialId::kWelcomeTourPrototype1),
                            /*element_context=*/_,
                            /*completed_callback=*/_,
                            /*aborted_callback=*/_))
      .WillOnce(MoveArgs<4>(&aborted_callback));
  EXPECT_CALL(*observer(), OnWelcomeTourStarted);
  SimulateNewUserFirstLogin(primary_account_id.GetUserEmail());
  Mock::VerifyAndClearExpectations(user_education_delegate());
  Mock::VerifyAndClearExpectations(observer());
  ASSERT_FALSE(aborted_callback.is_null());

  // Force tablet mode on, which should cause the tutorial to abort. Because
  // the device is in tablet mode, no attempt should be made to launch the
  // Explore app.
  EXPECT_CALL(*user_education_delegate(), AbortTutorial)
      .WillOnce(RunOnceClosure(std::move(aborted_callback)));
  EXPECT_CALL(*user_education_delegate(),
              LaunchSystemWebAppAsync(
                  Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
                  Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())))
      .Times(0);
  EXPECT_CALL(*observer(), OnWelcomeTourEnded);
  TabletMode::Get()->SetEnabledForTest(true);
  Mock::VerifyAndClearExpectations(observer());
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // Wait for the dialog widget to be destroyed.
  ASSERT_THAT(WelcomeTourDialog::Get(),
              Property(&WelcomeTourDialog::GetWidget, NotNull()));
  WidgetDestroyedWaiter(WelcomeTourDialog::Get()->GetWidget()).Wait();
  EXPECT_FALSE(WelcomeTourDialog::Get());
}

}  // namespace ash
