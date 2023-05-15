// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include <map>
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
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/tutorial_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash {
namespace {

// Aliases.
using ::session_manager::SessionState;
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Pair;
using ::testing::StrictMock;
using ::user_education::TutorialDescription;

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

MATCHER_P4(BubbleStep,
           help_bubble_id,
           element_specifier,
           body_text_id,
           has_next_button,
           "") {
  namespace util = user_education_util;
  return arg.step_type == ui::InteractionSequence::StepType::kShown &&
         util::GetHelpBubbleId(arg.extended_properties) == help_bubble_id &&
         arg.body_text_id == body_text_id &&
         arg.next_button_callback.is_null() != has_next_button &&
         absl::visit(base::Overloaded{
                         [&](const ui::ElementIdentifier& element_id) {
                           return arg.element_id == element_id &&
                                  arg.element_name.empty();
                         },
                         [&](const std::string& element_name) {
                           return arg.element_name == element_name &&
                                  arg.element_id == ui::ElementIdentifier();
                         },
                     },
                     element_specifier);
}

MATCHER_P2(EventStep, element_specifier, has_name_elements_callback, "") {
  return arg.step_type == ui::InteractionSequence::StepType::kCustomEvent &&
         arg.name_elements_callback.is_null() != has_name_elements_callback &&
         absl::visit(base::Overloaded{
                         [&](const ui::ElementIdentifier& element_id) {
                           return arg.element_id == element_id &&
                                  arg.element_name.empty();
                         },
                         [&](const std::string& element_name) {
                           return arg.element_name == element_name &&
                                  arg.element_id == ui::ElementIdentifier();
                         },
                     },
                     element_specifier);
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
                  BubbleStep(HelpBubbleId::kWelcomeTourShelf,
                             ElementSpecifier(kShelfViewElementId),
                             IDS_ASH_WELCOME_TOUR_SHELF_BUBBLE_BODY_TEXT,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kShelfViewElementId),
                            /*has_name_elements_callback=*/true),
                  BubbleStep(HelpBubbleId::kWelcomeTourStatusArea,
                             ElementSpecifier(kUnifiedSystemTrayElementName),
                             IDS_ASH_WELCOME_TOUR_STATUS_AREA_BUBBLE_BODY_TEXT,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kUnifiedSystemTrayElementName),
                            /*has_name_elements_callback=*/true),
                  BubbleStep(HelpBubbleId::kWelcomeTourHomeButton,
                             ElementSpecifier(kHomeButtonElementName),
                             IDS_ASH_WELCOME_TOUR_HOME_BUTTON_BUBBLE_BODY_TEXT,
                             /*has_next_button=*/true),
                  BubbleStep(HelpBubbleId::kWelcomeTourSearchBox,
                             ElementSpecifier(kSearchBoxViewElementId),
                             IDS_ASH_WELCOME_TOUR_SEARCH_BOX_BUBBLE_BODY_TEXT,
                             /*has_next_button=*/true),
                  BubbleStep(
                      HelpBubbleId::kWelcomeTourSettingsApp,
                      ElementSpecifier(kSettingsAppListItemViewElementId),
                      IDS_ASH_WELCOME_TOUR_SETTINGS_APP_BUBBLE_BODY_TEXT,
                      /*has_next_button=*/true),
                  BubbleStep(HelpBubbleId::kWelcomeTourExploreApp,
                             ElementSpecifier(kExploreAppListItemViewElementId),
                             IDS_ASH_WELCOME_TOUR_EXPLORE_APP_BUBBLE_BODY_TEXT,
                             /*has_next_button=*/false))))));
}

// Verifies that the Welcome Tour tutorial is started when the primary user
// session is first activated and then never again, as well as that start/end
// events are propagated to observers appropriately.
TEST_F(WelcomeTourControllerTest, StartsTutorialAndPropagatesEvents) {
  AccountId primary_account_id = AccountId::FromUserEmail("primary@test");
  AccountId secondary_account_id = AccountId::FromUserEmail("secondary@test");

  // Ensure controller exists.
  auto* welcome_tour_controller = WelcomeTourController::Get();
  ASSERT_TRUE(welcome_tour_controller);

  // Ensure delegate exists and disallow any unexpected tutorial starts.
  auto* user_education_delegate = this->user_education_delegate();
  ASSERT_TRUE(user_education_delegate);
  EXPECT_CALL(*user_education_delegate, StartTutorial).Times(0);

  // Observe the `WelcomeTourController` for start/end events.
  StrictMock<MockWelcomeTourControllerObserver> observer;
  base::ScopedObservation<WelcomeTourController, WelcomeTourControllerObserver>
      observation{&observer};
  observation.Observe(welcome_tour_controller);

  // Add a primary and secondary user session. This should *not* trigger the
  // Welcome Tour tutorial to start.
  auto* session_controller_client = GetSessionControllerClient();
  session_controller_client->AddUserSession(primary_account_id.GetUserEmail());
  session_controller_client->AddUserSession(
      secondary_account_id.GetUserEmail());

  // Activate the primary user session. This *should* trigger the Welcome Tour
  // tutorial to start as well as notify observers. Note that completed/aborted
  // callbacks are cached for later verification.
  base::OnceClosure ended_callbacks[2u];
  EXPECT_CALL(
      *user_education_delegate,
      StartTutorial(Eq(primary_account_id),
                    Eq(TutorialId::kWelcomeTourPrototype1),
                    Eq(welcome_tour_controller->GetInitialElementContext()),
                    /*completed_callback=*/_,
                    /*aborted_callback=*/_))
      .WillOnce(MoveArgs<3, 4>(&ended_callbacks[0u], &ended_callbacks[1u]));
  EXPECT_CALL(observer, OnWelcomeTourStarted);
  session_controller_client->SetSessionState(SessionState::ACTIVE);
  testing::Mock::VerifyAndClearExpectations(user_education_delegate);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Disallow any unexpected tutorial starts.
  EXPECT_CALL(*user_education_delegate, StartTutorial).Times(0);

  // Switch to the secondary user session and back again. This should *not*
  // trigger the Welcome Tour tutorial to start.
  session_controller_client->SwitchActiveUser(secondary_account_id);
  session_controller_client->SwitchActiveUser(primary_account_id);

  // Deactivate and then reactivate the primary user session. This should *not*
  // trigger the Welcome Tour tutorial to start.
  session_controller_client->SetSessionState(SessionState::LOCKED);
  session_controller_client->SetSessionState(SessionState::ACTIVE);

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

}  // namespace ash
