// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/user_education/user_education_constants.h"
#include "base/check_op.h"
#include "components/user_education/common/tutorial_description.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
WelcomeTourController* g_instance = nullptr;

}  // namespace

// WelcomeTourController -------------------------------------------------------

WelcomeTourController::WelcomeTourController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
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
          .emplace(std::piecewise_construct,
                   std::forward_as_tuple("AshWelcomeTourPrototype1"),
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

}  // namespace ash
