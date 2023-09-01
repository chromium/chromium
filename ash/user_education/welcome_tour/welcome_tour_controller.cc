// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/system/scoped_nudge_pause.h"
#include "ash/public/cpp/system/scoped_toast_pause.h"
#include "ash/public/cpp/system/system_nudge_pause_manager.h"
#include "ash/public/cpp/system/toast_manager.h"
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
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "ash/user_education/welcome_tour/welcome_tour_notification_blocker.h"
#include "ash/user_education/welcome_tour/welcome_tour_scrim.h"
#include "ash/user_education/welcome_tour/welcome_tour_window_minimizer.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/timer/elapsed_timer.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_manager/user_type.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
WelcomeTourController* g_instance = nullptr;

// Helpers ---------------------------------------------------------------------

user_education::HelpBubbleParams::ExtendedProperties
CreateHelpBubbleExtendedProperties(HelpBubbleId help_bubble_id) {
  return user_education_util::CreateExtendedProperties(
      user_education_util::CreateExtendedProperties(help_bubble_id),
      user_education_util::CreateExtendedProperties(ui::MODAL_TYPE_SYSTEM),
      user_education_util::CreateExtendedProperties(
          /*body_icon=*/gfx::kNoneIcon));
}

base::RepeatingCallback<void(ui::TrackedElement*)> DefaultNextButtonCallback() {
  return base::BindRepeating([](ui::TrackedElement* current_anchor) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        current_anchor, user_education::kHelpBubbleNextButtonClickedEvent);
  });
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

void LaunchExploreAppAsync(UserEducationPrivateApiKey key) {
  UserEducationController::Get()->LaunchSystemWebAppAsync(
      key, ash::SystemWebAppType::HELP,
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
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

  tutorial_description.complete_button_text_id =
      IDS_ASH_WELCOME_TOUR_COMPLETE_BUTTON_TEXT;

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
          .SetBubbleArrow(user_education::HelpBubbleArrow::kBottomCenter)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_SHELF_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourShelf))
          .AddCustomNextButton(DefaultNextButtonCallback().Then(
              base::BindRepeating(&WelcomeTourController::SetCurrentStep,
                                  weak_ptr_factory_.GetMutableWeakPtr(),
                                  welcome_tour_metrics::Step::kStatusArea))));

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
          .SetBubbleArrow(user_education::HelpBubbleArrow::kBottomRight)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_STATUS_AREA_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourStatusArea))
          .AddCustomNextButton(DefaultNextButtonCallback().Then(
              base::BindRepeating(&WelcomeTourController::SetCurrentStep,
                                  weak_ptr_factory_.GetMutableWeakPtr(),
                                  welcome_tour_metrics::Step::kHomeButton)))
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
          .SetBubbleArrow(user_education::HelpBubbleArrow::kBottomLeft)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_HOME_BUTTON_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourHomeButton))
          .AddCustomNextButton(base::BindRepeating([](ui::TrackedElement*) {
                                 Shell::Get()->app_list_controller()->Show(
                                     GetPrimaryDisplayId(),
                                     AppListShowSource::kWelcomeTour,
                                     ui::EventTimeForNow(),
                                     /*should_record_metrics=*/true);
                               })
                                   .Then(base::BindRepeating(
                                       &WelcomeTourController::SetCurrentStep,
                                       weak_ptr_factory_.GetMutableWeakPtr(),
                                       welcome_tour_metrics::Step::kSearch)))
          .InAnyContext());

  // Step 4: Search box.
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kSearchBoxViewElementId)
          .SetBubbleArrow(user_education::HelpBubbleArrow::kTopCenter)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_SEARCH_BOX_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourSearchBox))
          .AddCustomNextButton(DefaultNextButtonCallback().Then(
              base::BindRepeating(&WelcomeTourController::SetCurrentStep,
                                  weak_ptr_factory_.GetMutableWeakPtr(),
                                  welcome_tour_metrics::Step::kSettingsApp)))
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
          .SetBubbleArrow(user_education::HelpBubbleArrow::kBottomLeft)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_SETTINGS_APP_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourSettingsApp))
          .AddCustomNextButton(DefaultNextButtonCallback().Then(
              base::BindRepeating(&WelcomeTourController::SetCurrentStep,
                                  weak_ptr_factory_.GetMutableWeakPtr(),
                                  welcome_tour_metrics::Step::kExploreApp)))
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
          .SetBubbleArrow(user_education::HelpBubbleArrow::kBottomLeft)
          .SetBubbleBodyText(IDS_ASH_WELCOME_TOUR_EXPLORE_APP_BUBBLE_BODY_TEXT)
          .SetExtendedProperties(CreateHelpBubbleExtendedProperties(
              HelpBubbleId::kWelcomeTourExploreApp))
          .InSameContext());

  // Step 7: Explore app window.
  // Implemented in `WelcomeTourController::OnWelcomeTourEnded()`.

  return tutorial_descriptions_by_id;
}

void WelcomeTourController::OnAccessibilityControllerShutdown() {
  MaybeAbortWelcomeTour(welcome_tour_metrics::AbortedReason::kShutdown);
}

void WelcomeTourController::OnAccessibilityStatusChanged() {
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    MaybeAbortWelcomeTour(
        welcome_tour_metrics::AbortedReason::kChromeVoxEnabled);
  }
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

void WelcomeTourController::OnShellDestroying() {
  MaybeAbortWelcomeTour(welcome_tour_metrics::AbortedReason::kShutdown);
}

void WelcomeTourController::OnTabletControllerDestroyed() {
  MaybeAbortWelcomeTour(welcome_tour_metrics::AbortedReason::kShutdown);
}

void WelcomeTourController::OnTabletModeStarting() {
  MaybeAbortWelcomeTour(
      welcome_tour_metrics::AbortedReason::kTabletModeEnabled);
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

  if (!features::IsWelcomeTourForceUserEligibilityEnabled()) {
    // Welcome Tour is supported for regular users only.
    const auto* const session_controller = Shell::Get()->session_controller();
    if (const auto user_type = session_controller->GetUserType();
        user_type != user_manager::UserType::USER_TYPE_REGULAR) {
      welcome_tour_metrics::RecordTourPrevented(
          welcome_tour_metrics::PreventedReason::kUserTypeNotRegular);
      return;
    }

    // Welcome Tour is not supported for managed accounts.
    if (session_controller->IsActiveAccountManaged()) {
      welcome_tour_metrics::RecordTourPrevented(
          welcome_tour_metrics::PreventedReason::kManagedAccount);
      return;
    }

    // The cross-device proxy for whether the user is "new" or "existing" is
    // untested out in the wild. For sanity, confirm that the user is also
    // considered "new" locally in case the proxy check proves to be erroneous.
    if (!session_controller->IsUserFirstLogin()) {
      welcome_tour_metrics::RecordTourPrevented(
          welcome_tour_metrics::PreventedReason::kUserNotNewLocally);
      return;
    }

    const absl::optional<bool>& is_new_user =
        UserEducationController::Get()->IsNewUser(UserEducationPrivateApiKey());

    // If it is not known whether the user is "new" or "existing" when this code
    // is reached, the user is treated as "existing" since the Welcome Tour
    // cannot be delayed and we want to err on the side of being conservative.
    if (!is_new_user.has_value()) {
      welcome_tour_metrics::RecordTourPrevented(
          welcome_tour_metrics::PreventedReason::kUserNewnessNotAvailable);
      return;
    }

    // Welcome Tour is not supported for "existing" users.
    if (!is_new_user.value()) {
      welcome_tour_metrics::RecordTourPrevented(
          welcome_tour_metrics::PreventedReason::kUserNotNewCrossDevice);
      return;
    }
  }

  // We should attempt to launch the Explore app even if the Welcome Tour is
  // prevented provided that (a) the user is new, and (b) the device is not in
  // tablet mode. This is in keeping with existing first run behavior.
  base::ScopedClosureRunner maybe_launch_explore_app_async(
      TabletMode::IsInTabletMode()
          ? base::DoNothing()
          : base::BindOnce(&LaunchExploreAppAsync,
                           UserEducationPrivateApiKey()));

  // Welcome Tour is not supported with ChromeVox enabled.
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    welcome_tour_metrics::RecordTourPrevented(
        welcome_tour_metrics::PreventedReason::kChromeVoxEnabled);
    return;
  }

  // Welcome Tour is not supported in tablet mode.
  if (TabletMode::IsInTabletMode()) {
    welcome_tour_metrics::RecordTourPrevented(
        welcome_tour_metrics::PreventedReason::kTabletModeEnabled);
    return;
  }

  // Welcome Tour is not supported for counterfactual experiment arms.
  if (features::IsWelcomeTourEnabledCounterfactually()) {
    welcome_tour_metrics::RecordTourPrevented(
        welcome_tour_metrics::PreventedReason::kCounterfactualExperimentArm);
    return;
  }

  // The Welcome Tour is not being prevented, so hold off on opening the Explore
  // app until the Welcome Tour is either completed or aborted.
  std::ignore = maybe_launch_explore_app_async.Release();

  // NOTE: It is theoretically possible for the tutorial to outlive `this`
  // controller during the destruction sequence.
  UserEducationTutorialController::Get()->StartTutorial(
      UserEducationPrivateApiKey(), TutorialId::kWelcomeTourPrototype1,
      GetInitialElementContext(),
      /*completed_callback=*/
      base::BindOnce(&WelcomeTourController::OnWelcomeTourEnded,
                     weak_ptr_factory_.GetWeakPtr(), /*completed=*/true,
                     /*time_since_start=*/base::ElapsedTimer()),
      /*aborted_callback=*/
      base::BindOnce(&WelcomeTourController::OnWelcomeTourEnded,
                     weak_ptr_factory_.GetWeakPtr(), /*completed=*/false,
                     /*time_since_start=*/base::ElapsedTimer()));

  // The attempt to start the tutorial above is guaranteed to succeed or crash.
  // If this line of code is reached, the tour has indeed been started.
  OnWelcomeTourStarted();
}

void WelcomeTourController::MaybeAbortWelcomeTour(
    welcome_tour_metrics::AbortedReason reason) {
  if (aborted_reason_ == welcome_tour_metrics::AbortedReason::kUnknown) {
    aborted_reason_ = reason;
  }

  UserEducationTutorialController::Get()->AbortTutorial(
      UserEducationPrivateApiKey(), TutorialId::kWelcomeTourPrototype1);
}

// TODO(http://b/277091006): Stabilize app launches.
// TODO(http://b/277091067): Stabilize apps in launcher.
// TODO(http://b/277091443): Stabilize apps in shelf.
// TODO(http://b/277091733): Stabilize continue section in launcher.
// TODO(http://b/277091715): Stabilize pods in shelf.
// TODO(http://b/277091619): Stabilize wallpaper.
void WelcomeTourController::OnWelcomeTourStarted() {
  aborted_reason_ = welcome_tour_metrics::AbortedReason::kUnknown;
  accelerator_handler_ = std::make_unique<WelcomeTourAcceleratorHandler>(
      base::BindRepeating(&WelcomeTourController::MaybeAbortWelcomeTour,
                          weak_ptr_factory_.GetWeakPtr(),
                          welcome_tour_metrics::AbortedReason::kAccelerator));
  accessibility_observation_.Observe(Shell::Get()->accessibility_controller());
  notification_blocker_ = std::make_unique<WelcomeTourNotificationBlocker>();
  notification_blocker_->Init();
  nudge_pause_ = SystemNudgePauseManager::Get()->CreateScopedPause();
  scrim_ = std::make_unique<WelcomeTourScrim>();
  shell_observation_.Observe(Shell::Get());
  tablet_mode_observation_.Observe(TabletMode::Get());
  toast_pause_ = ToastManager::Get()->CreateScopedPause();
  window_minimizer_ = std::make_unique<WelcomeTourWindowMinimizer>();

  // NOTE: The accept button doesn't need to be explicitly handled because the
  // Welcome Tour will automatically proceed to the next step once the dialog is
  // closed unless it has been aborted.
  WelcomeTourDialog::CreateAndShow(
      /*accept_callback=*/base::BindOnce(&WelcomeTourController::SetCurrentStep,
                                         weak_ptr_factory_.GetMutableWeakPtr(),
                                         welcome_tour_metrics::Step::kShelf),
      /*cancel_callback=*/
      base::BindOnce(&WelcomeTourController::MaybeAbortWelcomeTour,
                     weak_ptr_factory_.GetWeakPtr(),
                     welcome_tour_metrics::AbortedReason::kUserDeclinedTour),
      /*close_callback=*/
      base::BindOnce(&WelcomeTourController::MaybeAbortWelcomeTour,
                     weak_ptr_factory_.GetWeakPtr(),
                     welcome_tour_metrics::AbortedReason::kUnknown));

  SetCurrentStep(welcome_tour_metrics::Step::kDialog);

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
void WelcomeTourController::OnWelcomeTourEnded(
    bool completed,
    base::ElapsedTimer time_since_start) {
  accelerator_handler_.reset();
  accessibility_observation_.Reset();
  notification_blocker_.reset();
  nudge_pause_.reset();
  scrim_.reset();
  shell_observation_.Reset();
  tablet_mode_observation_.Reset();
  toast_pause_.reset();
  window_minimizer_.reset();

  if (!completed) {
    welcome_tour_metrics::RecordTourAborted(aborted_reason_);

    // `current_step_` may not be set in testing.
    if (current_step_.has_value()) {
      welcome_tour_metrics::RecordStepAborted(current_step_.value());
    } else {
      CHECK_IS_TEST();
    }

    if (auto* dialog = WelcomeTourDialog::Get()) {
      // Ensure the Welcome Tour dialog is closed when the tour is aborted since
      // the abort could have originated from outside of the dialog itself. Note
      // that weak pointers are invalidated to avoid doing work on widget close.
      if (auto* widget = dialog->GetWidget(); widget && !widget->IsClosed()) {
        weak_ptr_factory_.InvalidateWeakPtrs();
        widget->Close();
      }
    }
  }

  // Attempt to launch the Explore app regardless of tour completion so long as
  // the device is not in tablet mode. This is in keeping with existing first
  // run behavior.
  if (!TabletMode::IsInTabletMode()) {
    LaunchExploreAppAsync(UserEducationPrivateApiKey());
    SetCurrentStep(welcome_tour_metrics::Step::kExploreAppWindow);
  }

  SetCurrentStep(absl::nullopt);

  welcome_tour_metrics::RecordTourDuration(time_since_start.Elapsed(),
                                           completed);

  for (auto& observer : observer_list_) {
    observer.OnWelcomeTourEnded();
  }
}

void WelcomeTourController::SetCurrentStep(
    absl::optional<welcome_tour_metrics::Step> step) {
  if (current_step_) {
    welcome_tour_metrics::RecordStepDuration(current_step_.value(),
                                             current_step_timer_.Elapsed());
  }

  if (step) {
    welcome_tour_metrics::RecordStepShown(step.value());
  }

  current_step_ = step;
  current_step_timer_ = base::ElapsedTimer();
}

}  // namespace ash
