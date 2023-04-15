// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/user_education/user_education_constants.h"
#include "ash/user_education/user_education_controller.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller_observer.h"
#include "base/check_op.h"
#include "components/user_education/common/tutorial_description.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
WelcomeTourController* g_instance = nullptr;

// Helpers ---------------------------------------------------------------------

aura::Window* GetPrimaryRootWindow() {
  auto* window_tree_host_manager = Shell::Get()->window_tree_host_manager();
  return window_tree_host_manager
             ? window_tree_host_manager->GetPrimaryRootWindow()
             : nullptr;
}

Shelf* GetPrimaryShelf() {
  auto* primary_root_window = GetPrimaryRootWindow();
  return primary_root_window ? Shelf::ForWindow(primary_root_window) : nullptr;
}

HotseatWidget* GetPrimaryHotseatWidget() {
  auto* primary_shelf = GetPrimaryShelf();
  return primary_shelf ? primary_shelf->hotseat_widget() : nullptr;
}

ShelfView* GetPrimaryShelfView() {
  auto* primary_hotseat_widget = GetPrimaryHotseatWidget();
  return primary_hotseat_widget ? primary_hotseat_widget->GetShelfView()
                                : nullptr;
}

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

void WelcomeTourController::AddObserver(
    WelcomeTourControllerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void WelcomeTourController::RemoveObserver(
    WelcomeTourControllerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

ui::ElementContext WelcomeTourController::GetInitialElementContext() const {
  return views::ElementTrackerViews::GetContextForView(GetPrimaryShelfView());
}

// TODO(http://b/275616974): Implement tutorial descriptions.
std::map<TutorialId, user_education::TutorialDescription>
WelcomeTourController::GetTutorialDescriptions() {
  std::map<TutorialId, user_education::TutorialDescription>
      tutorial_descriptions_by_id;

  user_education::TutorialDescription& tutorial_description =
      tutorial_descriptions_by_id
          .emplace(std::piecewise_construct,
                   std::forward_as_tuple(TutorialId::kWelcomeTourPrototype1),
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

  // NOTE: It is theoretically possible for the tutorial to outlive `this`
  // controller during the destruction sequence.
  UserEducationController::Get()->StartTutorial(
      UserEducationPrivateApiKey(), TutorialId::kWelcomeTourPrototype1,
      GetInitialElementContext(),
      /*completed_callback=*/
      base::BindOnce(&WelcomeTourController::OnWelcomeTourEnded,
                     weak_ptr_factory_.GetWeakPtr()),
      /*aborted_callback=*/
      base::BindOnce(&WelcomeTourController::OnWelcomeTourEnded,
                     weak_ptr_factory_.GetWeakPtr()));

  // The attempt to start the tutorial above is guaranteed to succeed or crash.
  // If this line of code is reached, the tutorial has indeed been started.
  OnWelcomeTourStarted();
}

// TODO(http://b/277091650): Show scrim.
// TODO(http://b/277091006): Stabilize app launches.
// TODO(http://b/277091067): Stabilize apps in launcher.
// TODO(http://b/277091443): Stabilize apps in shelf.
// TODO(http://b/277091733): Stabilize continue section in launcher.
// TODO(http://b/277091715): Stabilize pods in shelf.
// TODO(http://b/277091619): Stabilize wallpaper.
// TODO(http://b/277091643): Stabilize notifications.
// TODO(http://b/277091624): Stabilize nudges/toasts.
void WelcomeTourController::OnWelcomeTourStarted() {
  for (auto& observer : observer_list_) {
    observer.OnWelcomeTourStarted();
  }
}
// TODO(http://b/277091650): Hide scrim.
// TODO(http://b/277091006): Restore app launches.
// TODO(http://b/277091067): Restore apps in launcher.
// TODO(http://b/277091443): Restore apps in shelf.
// TODO(http://b/277091733): Restore continue section in launcher.
// TODO(http://b/277091715): Restore pods in shelf.
// TODO(http://b/277091619): Restore wallpaper.
// TODO(http://b/277091643): Restore notifications.
// TODO(http://b/277091624): Restore nudges/toasts.
void WelcomeTourController::OnWelcomeTourEnded() {
  for (auto& observer : observer_list_) {
    observer.OnWelcomeTourEnded();
  }
}

}  // namespace ash
