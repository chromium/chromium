// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include <map>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_constants.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/welcome_tour/mock_welcome_tour_controller_observer.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller_observer.h"
#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"
#include "ash/user_education/welcome_tour/welcome_tour_test_util.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/tutorial_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

// Aliases.
using ::base::test::RunOnceClosure;
using ::session_manager::SessionState;
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matches;
using ::testing::Pair;
using ::testing::StrictMock;
using ::user_education::HelpBubbleArrow;
using ::user_education::TutorialDescription;

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
  return arg.step_type() == ui::InteractionSequence::StepType::kShown &&
         Matches(ElementSpecifierEq(element_specifier))(arg) &&
         arg.context_mode() == context_mode &&
         util::GetHelpBubbleId(arg.extended_properties()) == help_bubble_id &&
         arg.body_text_id() == body_text_id && arg.arrow() == arrow &&
         arg.next_button_callback().is_null() != has_next_button;
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
          Field(
              &TutorialDescription::steps,
              ElementsAre(
                  BubbleStep(ElementSpecifier(kShelfViewElementId),
                             ContextMode::kInitial,
                             HelpBubbleId::kWelcomeTourShelf,
                             IDS_ASH_WELCOME_TOUR_SHELF_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kShelfViewElementId),
                            ContextMode::kFromPreviousStep,
                            /*has_name_elements_callback=*/true),
                  BubbleStep(ElementSpecifier(kUnifiedSystemTrayElementName),
                             ContextMode::kAny,
                             HelpBubbleId::kWelcomeTourStatusArea,
                             IDS_ASH_WELCOME_TOUR_STATUS_AREA_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kUnifiedSystemTrayElementName),
                            ContextMode::kFromPreviousStep,
                            /*has_name_elements_callback=*/true),
                  BubbleStep(ElementSpecifier(kHomeButtonElementName),
                             ContextMode::kAny,
                             HelpBubbleId::kWelcomeTourHomeButton,
                             IDS_ASH_WELCOME_TOUR_HOME_BUTTON_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  BubbleStep(ElementSpecifier(kSearchBoxViewElementId),
                             ContextMode::kAny,
                             HelpBubbleId::kWelcomeTourSearchBox,
                             IDS_ASH_WELCOME_TOUR_SEARCH_BOX_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kSearchBoxViewElementId),
                            ContextMode::kFromPreviousStep,
                            /*has_name_elements_callback=*/false),
                  BubbleStep(ElementSpecifier(kSettingsAppElementId),
                             ContextMode::kFromPreviousStep,
                             HelpBubbleId::kWelcomeTourSettingsApp,
                             IDS_ASH_WELCOME_TOUR_SETTINGS_APP_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kSettingsAppElementId),
                            ContextMode::kFromPreviousStep,
                            /*has_name_elements_callback=*/false),
                  BubbleStep(ElementSpecifier(kExploreAppElementId),
                             ContextMode::kFromPreviousStep,
                             HelpBubbleId::kWelcomeTourExploreApp,
                             IDS_ASH_WELCOME_TOUR_EXPLORE_APP_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/false))))));
}

// Verifies that the Welcome Tour is started when the primary user session is
// first activated and then never again, as well as that start/end events are
// propagated to observers appropriately.
TEST_F(WelcomeTourControllerTest, StartsTourAndPropagatesEvents) {
  const AccountId primary_account_id = AccountId::FromUserEmail("primary@test");
  const AccountId secondary_account_id =
      AccountId::FromUserEmail("secondary@test");

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

  // Add a primary and secondary user session. This should *not* trigger the
  // Welcome Tour to start.
  auto* const session_controller_client = GetSessionControllerClient();
  session_controller_client->AddUserSession(primary_account_id.GetUserEmail());
  session_controller_client->AddUserSession(
      secondary_account_id.GetUserEmail());

  // Activate the primary user session. The shown dialog marks the start of the
  // Welcome Tour and the observers are notified.
  EXPECT_CALL(observer, OnWelcomeTourStarted);
  session_controller_client->SetSessionState(SessionState::ACTIVE);
  EXPECT_TRUE(WelcomeTourDialog::Get());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Click `accept_button`. This *should* trigger the Welcome Tour tutorial to
  // start. Note that the tutorial completed/aborted callbacks are cached for
  // later verification.
  base::OnceClosure ended_callbacks[2u];
  EXPECT_CALL(
      *user_education_delegate,
      StartTutorial(Eq(primary_account_id),
                    Eq(TutorialId::kWelcomeTourPrototype1),
                    Eq(welcome_tour_controller->GetInitialElementContext()),
                    /*completed_callback=*/_,
                    /*aborted_callback=*/_))
      .WillOnce(MoveArgs<3, 4>(&ended_callbacks[0u], &ended_callbacks[1u]));
  const views::View* const accept_button = GetDialogAcceptButton();
  ASSERT_TRUE(accept_button);
  LeftClickOn(accept_button);
  testing::Mock::VerifyAndClearExpectations(user_education_delegate);

  // Wait until `welcome_tour_dialog` gets destroyed.
  views::test::WidgetDestroyedWaiter(WelcomeTourDialog::Get()->GetWidget())
      .Wait();
  EXPECT_FALSE(WelcomeTourDialog::Get());

  // Disallow any unexpected tutorial starts.
  EXPECT_CALL(*user_education_delegate, StartTutorial).Times(0);

  // Switch to the secondary user session and back again. This should *not*
  // either show the dialog or start the Welcome Tour tutorial.
  session_controller_client->SwitchActiveUser(secondary_account_id);
  EXPECT_FALSE(WelcomeTourDialog::Get());
  session_controller_client->SwitchActiveUser(primary_account_id);
  EXPECT_FALSE(WelcomeTourDialog::Get());

  // Deactivate and then reactivate the primary user session. This should *not*
  // either show the dialog or start the Welcome Tour tutorial.
  session_controller_client->SetSessionState(SessionState::LOCKED);
  EXPECT_FALSE(WelcomeTourDialog::Get());
  session_controller_client->SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(WelcomeTourDialog::Get());

  // Verify that the same event is propagated to observers regardless of whether
  // user education services in the browser indicate the tour was completed or
  // aborted.
  for (base::OnceClosure& ended_callback : ended_callbacks) {
    ASSERT_FALSE(ended_callback.is_null());
    EXPECT_CALL(observer, OnWelcomeTourEnded);
    std::move(ended_callback).Run();
    testing::Mock::VerifyAndClearExpectations(&observer);
  }
}

// Verifies that the Welcome Tour ends without starting the tutorial after
// clicking the dialog cancel button.
TEST_F(WelcomeTourControllerTest, CancelsTourAndPropagatesEvents) {
  SimulateUserLogin("primary@test");

  // Observe the `WelcomeTourController` for end events.
  StrictMock<MockWelcomeTourControllerObserver> observer;
  base::ScopedObservation<WelcomeTourController, WelcomeTourControllerObserver>
      observation{&observer};
  observation.Observe(WelcomeTourController::Get());

  base::test::TestFuture<void> ended_future;
  EXPECT_CALL(observer, OnWelcomeTourEnded)
      .WillOnce(RunOnceClosure(ended_future.GetCallback()));

  const views::View* const cancel_button = GetDialogCancelButton();
  ASSERT_TRUE(cancel_button);
  LeftClickOn(cancel_button);
  EXPECT_TRUE(ended_future.Wait());
}

// WelcomeTourControllerRunTest ------------------------------------------------

// Base class for tests of the `WelcomeTourController` that run the Welcome
// Tour in order to assert expectations before, during, and/or after run time.
class WelcomeTourControllerRunTest : public WelcomeTourControllerTest {
 public:
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

    // When the Welcome Tour tutorial is started, cache the callback to invoke
    // to complete the tutorial.
    base::OnceClosure completed_callback;
    EXPECT_CALL(
        *delegate,
        StartTutorial(_, Eq(TutorialId::kWelcomeTourPrototype1), _, _, _))
        .WillOnce(MoveArg<3>(&completed_callback));

    // Simulate login of the primary user. Note that this should trigger the
    // Welcome Tour to start automatically.
    SimulateUserLogin("primary@test");
    EXPECT_TRUE(started_future.Wait());

    // Click the dialog's accept button to start the tutorial.
    const views::View* const accept_button = GetDialogAcceptButton();
    ASSERT_TRUE(accept_button);
    LeftClickOn(accept_button);

    // Invoke the `in_progress_callback` so that tests can assert expectations
    // while the Welcome Tour is in progress.
    std::move(in_progress_callback).Run();

    // Complete the tutorial by invoking the cached callback.
    std::move(completed_callback).Run();
    EXPECT_TRUE(ended_future.Wait());
  }
};

// Tests -----------------------------------------------------------------------

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

}  // namespace ash
