// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/user_education/user_education_controller.h"
#include "ash/user_education/user_education_tutorial_controller.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/welcome_tour/welcome_tour_accelerator_handler.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller_observer.h"
#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"
#include "ash/user_education/welcome_tour/welcome_tour_notification_blocker.h"
#include "ash/user_education/welcome_tour/welcome_tour_scrim.h"
#include "ash/user_education/welcome_tour/welcome_tour_window_minimizer.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/tutorial_description.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
WelcomeTourController* g_instance = nullptr;

// Helpers ---------------------------------------------------------------------

user_education::HelpBubbleParams::ExtendedProperties
CreateHelpBubbleExtendedProperties(HelpBubbleId bubble_id) {
  return user_education_util::CreateExtendedProperties(
      user_education_util::CreateExtendedProperties(bubble_id),
      user_education_util::CreateExtendedProperties(
          ui::ModalType::MODAL_TYPE_SYSTEM));
}

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

views::View* GetMatchingViewInPrimaryRootWindow(
    ui::ElementIdentifier element_id) {
  return user_education_util::GetMatchingViewInRootWindow(GetPrimaryDisplayId(),
                                                          element_id);
}

views::TrackedElementViews* GetMatchingElementInPrimaryRootWindow(
    ui::ElementIdentifier element_id) {
  return views::ElementTrackerViews::GetInstance()->GetElementForView(
      GetMatchingViewInPrimaryRootWindow(element_id));
}

user_education::TutorialDescription::NameElementsCallback
NameMatchingElementInPrimaryRootWindowCallback(ui::ElementIdentifier element_id,
                                               const char* element_name) {
  return base::BindRepeating(
      [](ui::ElementIdentifier element_id, const char* element_name,
         ui::InteractionSequence* sequence, ui::TrackedElement*) {
        if (auto* element = GetMatchingElementInPrimaryRootWindow(element_id)) {
          sequence->NameElement(element, base::StringPiece(element_name));
          return true;
        }
        return false;
      },
      element_id, element_name);
}

}  // namespace

// WelcomeTourController -------------------------------------------------------

WelcomeTourController::WelcomeTourController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  session_observation_.Observe(Shell::Get()->session_controller());
  MaybeStartWelcomeTour();
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
  // NOTE: Don't use `GetMatchingElementInPrimaryRootWindow()` here as
  // `views::TrackedElementViews` only exist while views are shown and that may
  // not be the case when this method is called.
  return views::ElementTrackerViews::GetContextForView(
      GetMatchingViewInPrimaryRootWindow(kShelfViewElementId));
}

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

  // Step 0: Dialog.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::HiddenStep::WaitForShown(
          kWelcomeTourDialogElementId)
          .InAnyContext());

  // Wait for the dialog to be hidden before proceeding to the next bubble step.
  // Note that if the dialog is closed without the user having accepted it, the
  // Welcome Tour will be aborted and the next bubble step will not be reached.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::HiddenStep::WaitForHidden(
          kWelcomeTourDialogElementId)
          .InSameContext());

  // Step 1: Shelf.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kShelfViewElementId)
          .SetBubbleArrow(user_education::HelpBubbleArrow::kTopRight)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_SHELF_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourShelf))
          .AddDefaultNextButton());

  // Wait for "Next" button click before proceeding to the next bubble step.
  // NOTE: This event step also ensures that the next bubble step will show on
  // the primary display by naming the primary root window's status area.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::EventStep(
          user_education::kHelpBubbleNextButtonClickedEvent,
          kShelfViewElementId)
          .NameElements(NameMatchingElementInPrimaryRootWindowCallback(
              kUnifiedSystemTrayElementId, kUnifiedSystemTrayElementName))
          .InSameContext());

  // Step 2: Status area.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(
          kUnifiedSystemTrayElementName)
          .SetBubbleArrow(user_education::HelpBubbleArrow::kTopRight)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_STATUS_AREA_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourStatusArea))
          .AddDefaultNextButton()
          .InAnyContext());

  // Wait for "Next" button click before proceeding to the next bubble step.
  // NOTE: This event step also ensures that the next bubble step will show on
  // the primary display by naming the primary root window's home button.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::EventStep(
          user_education::kHelpBubbleNextButtonClickedEvent,
          kUnifiedSystemTrayElementName)
          .NameElements(NameMatchingElementInPrimaryRootWindowCallback(
              kHomeButtonElementId, kHomeButtonElementName))
          .InSameContext());

  // Step 3: Home button.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kHomeButtonElementName)
          .SetBubbleArrow(user_education::HelpBubbleArrow::kTopRight)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_HOME_BUTTON_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourHomeButton))
          .AddCustomNextButton(base::BindRepeating([](ui::TrackedElement*) {
            Shell::Get()->app_list_controller()->Show(
                GetPrimaryDisplayId(), AppListShowSource::kWelcomeTour,
                ui::EventTimeForNow(),
                /*should_record_metrics=*/true);
          }))
          .InAnyContext());

  // Step 4: Search box.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kSearchBoxViewElementId)
          .SetBubbleArrow(user_education::HelpBubbleArrow::kTopRight)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_SEARCH_BOX_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourSearchBox))
          .AddDefaultNextButton()
          .InAnyContext());

  // Wait for "Next" button click before proceeding to the next bubble step.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::EventStep(
          user_education::kHelpBubbleNextButtonClickedEvent,
          kSearchBoxViewElementId)
          .InSameContext());

  // Step 5: Settings app.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kSettingsAppElementId)
          .SetBubbleArrow(user_education::HelpBubbleArrow::kTopRight)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_SETTINGS_APP_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourSettingsApp))
          .AddDefaultNextButton()
          .InSameContext());

  // Wait for "Next" button click before proceeding to the next bubble step.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::EventStep(
          user_education::kHelpBubbleNextButtonClickedEvent,
          kSettingsAppElementId)
          .InSameContext());

  // Step 6: Explore app.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kExploreAppElementId)
          .SetBubbleArrow(user_education::HelpBubbleArrow::kTopRight)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_EXPLORE_APP_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourExploreApp))
          .InSameContext());

  // Step 7: Explore app window.
  // Implemented in `WelcomeTourController::OnWelcomeTourEnded()`.

  return tutorial_descriptions_by_id;
}

void WelcomeTourController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  MaybeStartWelcomeTour();
}

void WelcomeTourController::OnChromeTerminating() {
  session_observation_.Reset();
}

void WelcomeTourController::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  MaybeStartWelcomeTour();
}

void WelcomeTourController::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

void WelcomeTourController::OnTabletModeStarting() {
  MaybeAbortWelcomeTour();
}

void WelcomeTourController::MaybeStartWelcomeTour() {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  if (!user_education_util::IsPrimaryAccountActive()) {
    return;
  }

  // We can stop observations since we only observe sessions in order to start
  // the tour when the primary user session is activated for the first time.
  session_observation_.Reset();

  // Welcome Tour is not supported for "existing" users.
  if (!features::IsWelcomeTourForceUserEligibilityEnabled()) {
    // TODO(http://b/291970413): Enforce user eligibility requirements.
    NOTIMPLEMENTED();
  }

  // Welcome Tour is not supported in tablet mode.
  if (TabletMode::IsInTabletMode()) {
    return;
  }

  // NOTE: It is theoretically possible for the tutorial to outlive `this`
  // controller during the destruction sequence.
  UserEducationTutorialController::Get()->StartTutorial(
      UserEducationPrivateApiKey(), TutorialId::kWelcomeTourPrototype1,
      GetInitialElementContext(),
      /*completed_callback=*/
      base::BindOnce(&WelcomeTourController::OnWelcomeTourEnded,
                     weak_ptr_factory_.GetWeakPtr(), /*completed=*/true),
      /*aborted_callback=*/
      base::BindOnce(&WelcomeTourController::OnWelcomeTourEnded,
                     weak_ptr_factory_.GetWeakPtr(), /*completed=*/false));

  // The attempt to start the tutorial above is guaranteed to succeed or crash.
  // If this line of code is reached, the tour has indeed been started.
  OnWelcomeTourStarted();
}

void WelcomeTourController::MaybeAbortWelcomeTour() {
  UserEducationTutorialController::Get()->AbortTutorial(
      UserEducationPrivateApiKey(), TutorialId::kWelcomeTourPrototype1);
}

// TODO(http://b/277091006): Stabilize app launches.
// TODO(http://b/277091067): Stabilize apps in launcher.
// TODO(http://b/277091443): Stabilize apps in shelf.
// TODO(http://b/277091733): Stabilize continue section in launcher.
// TODO(http://b/277091715): Stabilize pods in shelf.
// TODO(http://b/277091619): Stabilize wallpaper.
// TODO(http://b/277091624): Stabilize nudges/toasts.
void WelcomeTourController::OnWelcomeTourStarted() {
  accelerator_handler_ = std::make_unique<WelcomeTourAcceleratorHandler>(
      base::BindRepeating(&WelcomeTourController::MaybeAbortWelcomeTour,
                          weak_ptr_factory_.GetWeakPtr()));

  notification_blocker_ = std::make_unique<WelcomeTourNotificationBlocker>();
  notification_blocker_->Init();

  scrim_ = std::make_unique<WelcomeTourScrim>();
  tablet_mode_observation_.Observe(TabletMode::Get());
  window_minimizer_ = std::make_unique<WelcomeTourWindowMinimizer>();

  // NOTE: The accept button doesn't need to be explicitly handled because the
  // Welcome Tour will automatically proceed to the next step once the dialog is
  // closed unless it has been aborted.
  WelcomeTourDialog::CreateAndShow(
      /*accept_callback=*/base::DoNothing(),
      /*cancel_callback=*/
      base::BindOnce(&WelcomeTourController::MaybeAbortWelcomeTour,
                     weak_ptr_factory_.GetWeakPtr()),
      /*close_callback=*/
      base::BindOnce(&WelcomeTourController::MaybeAbortWelcomeTour,
                     weak_ptr_factory_.GetWeakPtr()));

  for (auto& observer : observer_list_) {
    observer.OnWelcomeTourStarted();
  }
}

// TODO(http://b/277091006): Restore app launches.
// TODO(http://b/277091067): Restore apps in launcher.
// TODO(http://b/277091443): Restore apps in shelf.
// TODO(http://b/277091733): Restore continue section in launcher.
// TODO(http://b/277091715): Restore pods in shelf.
// TODO(http://b/277091619): Restore wallpaper.
// TODO(http://b/277091624): Restore nudges/toasts.
void WelcomeTourController::OnWelcomeTourEnded(bool completed) {
  accelerator_handler_.reset();
  notification_blocker_.reset();
  scrim_.reset();
  tablet_mode_observation_.Reset();
  window_minimizer_.reset();

  if (completed) {
    // Attempt to launch the Explore app on successful completion of the tour.
    UserEducationController::Get()->LaunchSystemWebAppAsync(
        UserEducationPrivateApiKey(), ash::SystemWebAppType::HELP,
        display::Screen::GetScreen()->GetPrimaryDisplay().id());
  } else if (auto* dialog = WelcomeTourDialog::Get()) {
    // Ensure the Welcome Tour dialog is closed when the tour is aborted since
    // the abort could have originated from outside of the dialog itself. Note
    // that weak pointers are invalidated to avoid doing work on widget close.
    if (auto* widget = dialog->GetWidget(); widget && !widget->IsClosed()) {
      weak_ptr_factory_.InvalidateWeakPtrs();
      widget->Close();
    }
  }

  for (auto& observer : observer_list_) {
    observer.OnWelcomeTourEnded();
  }
}

}  // namespace ash
