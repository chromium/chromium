// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/user_education/user_education_constants.h"
#include "ash/user_education/user_education_controller.h"
#include "ash/user_education/user_education_util.h"
#include "base/check_op.h"
#include "components/user_education/common/tutorial_description.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
WelcomeTourController* g_instance = nullptr;

// Unique identifier for the Welcome Tour tutorial.
constexpr char kTutorialId[] = "AshWelcomeTourPrototype1";

}  // namespace

// WelcomeTourController -------------------------------------------------------

WelcomeTourController::WelcomeTourController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  session_observation_.Observe(Shell::Get()->session_controller());
  MaybeStartTutorial();
}

WelcomeTourController::~WelcomeTourController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
WelcomeTourController* WelcomeTourController::Get() {
  return g_instance;
}

// TODO(http://b/275616974): Implement tutorial descriptions.
std::map<user_education::TutorialIdentifier,
         user_education::TutorialDescription>
WelcomeTourController::GetTutorialDescriptions() {
  std::map<user_education::TutorialIdentifier,
           user_education::TutorialDescription>
      tutorial_descriptions_by_id;

  user_education::TutorialDescription& tutorial_description =
      tutorial_descriptions_by_id
          .emplace(std::piecewise_construct, std::forward_as_tuple(kTutorialId),
                   std::forward_as_tuple())
          .first->second;

  // Step 1: Shelf.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kShelfViewElementId)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_SHELF_BUBBLE_BODY_TEXT)
          .AddDefaultNextButton());

  // Step 2: Status area.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(
          kUnifiedSystemTrayElementId)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_STATUS_AREA_BUBBLE_BODY_TEXT)
          .AddDefaultNextButton());

  // Step 3: Home button.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kHomeButtonElementId)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_HOME_BUTTON_BUBBLE_BODY_TEXT)
          .AddDefaultNextButton());

  // Step 4: Search box.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kSearchBoxViewElementId)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_SEARCH_BOX_BUBBLE_BODY_TEXT)
          .AddDefaultNextButton());

  // Step 5: Settings app.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(
          kSettingsAppListItemViewElementId)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_SETTINGS_APP_BUBBLE_BODY_TEXT)
          .AddDefaultNextButton());

  // Step 6: Explore app.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(
          kExploreAppListItemViewElementId)
          .SetBubbleBodyText(
              IDS_ASH_WELCOME_TOUR_EXPLORE_APP_BUBBLE_BODY_TEXT));

  return tutorial_descriptions_by_id;
}

void WelcomeTourController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  MaybeStartTutorial();
}

void WelcomeTourController::OnChromeTerminating() {
  session_observation_.Reset();
}

void WelcomeTourController::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  MaybeStartTutorial();
}

void WelcomeTourController::MaybeStartTutorial() {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  if (!user_education_util::IsPrimaryAccountActive()) {
    return;
  }

  // We can stop observations since we only observe sessions in order to start
  // the tutorial when the primary user session is activated for the first time.
  session_observation_.Reset();

  // TODO(http://b/275616974): Use production context after registering views.
  UserEducationController::Get()->StartTutorial(
      UserEducationPrivateApiKey(), kTutorialId, ui::ElementContext(),
      /*completed_callback=*/base::DoNothing(),
      /*aborted_callback=*/base::DoNothing());
}

}  // namespace ash
