// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_ui_controller_impl.h"

#include <optional>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/histogram_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

PrefService* pref_service() {
  auto* result =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  DCHECK(result);
  return result;
}

// Toast -----------------------------------------------------------------------

constexpr char kUnboundServiceToastId[] =
    "assistant_controller_unbound_service";

void ShowToast(const std::string& id,
               ToastCatalogName catalog_name,
               int message_id) {
  ToastData toast(id, catalog_name, l10n_util::GetStringUTF16(message_id));
  Shell::Get()->toast_manager()->Show(std::move(toast));
}

}  // namespace

// AssistantUiControllerImpl ---------------------------------------------------

AssistantUiControllerImpl::AssistantUiControllerImpl(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {
  model_.AddObserver(this);
  assistant_controller_observation_.Observe(AssistantController::Get());
  overview_controller_observation_.Observe(Shell::Get()->overview_controller());
}

AssistantUiControllerImpl::~AssistantUiControllerImpl() {
  model_.RemoveObserver(this);
}

// static
void AssistantUiControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kAssistantNumSessionsWhereOnboardingShown, 0);
}

void AssistantUiControllerImpl::SetAssistant(assistant::Assistant* assistant) {
  assistant_ = assistant;
}

const AssistantUiModel* AssistantUiControllerImpl::GetModel() const {
  return &model_;
}

int AssistantUiControllerImpl::GetNumberOfSessionsWhereOnboardingShown() const {
  return pref_service()->GetInteger(
      prefs::kAssistantNumSessionsWhereOnboardingShown);
}

bool AssistantUiControllerImpl::HasShownOnboarding() const {
  return has_shown_onboarding_;
}

void AssistantUiControllerImpl::SetKeyboardTraversalMode(
    bool keyboard_traversal_mode) {
  model_.SetKeyboardTraversalMode(keyboard_traversal_mode);
}

void AssistantUiControllerImpl::ShowUi(AssistantEntryPoint entry_point) {
  // Skip if the opt-in window is active.
  auto* assistant_setup = AssistantSetup::GetInstance();
  if (assistant_setup && assistant_setup->BounceOptInWindowIfActive())
    return;

  auto* assistant_state = AssistantState::Get();

  if (!assistant_state->settings_enabled().value_or(false) ||
      assistant_state->locked_full_screen_enabled().value_or(false)) {
    return;
  }

  // TODO(dmblack): Show a more helpful message to the user.
  if (assistant_state->assistant_status() ==
      assistant::AssistantStatus::NOT_READY) {
    ShowUnboundErrorToast();
    return;
  }

  if (!assistant_) {
    ShowUnboundErrorToast();
    return;
  }

  model_.SetVisible(entry_point);
}

std::optional<base::ScopedClosureRunner> AssistantUiControllerImpl::CloseUi(
    AssistantExitPoint exit_point) {
  if (model_.visibility() != AssistantVisibility::kVisible)
    return std::nullopt;

  // Set visibility to `kClosing`.
  model_.SetClosing(exit_point);

  // When the return value is destroyed, visibility will be set to `kClosed`
  // provided the visibility change hasn't been invalidated.
  return base::ScopedClosureRunner(base::BindOnce(
      [](const base::WeakPtr<AssistantUiControllerImpl>& weak_ptr,
         assistant::AssistantExitPoint exit_point) {
        if (weak_ptr)
          weak_ptr->model_.SetClosed(exit_point);
      },
      weak_factory_for_delayed_visibility_changes_.GetWeakPtr(), exit_point));
}

void AssistantUiControllerImpl::SetAppListBubbleWidth(int width) {
  model_.SetAppListBubbleWidth(width);
}

void AssistantUiControllerImpl::ToggleUi(
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  // When not visible, toggling will show the UI.
  if (model_.visibility() != AssistantVisibility::kVisible) {
    DCHECK(entry_point.has_value());
    ShowUi(entry_point.value());
    return;
  }

  // Otherwise toggling closes the UI.
  DCHECK(exit_point.has_value());
  CloseUi(exit_point.value());
}

void AssistantUiControllerImpl::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kActive)
    return;

  // If there is an active interaction, we need to show Assistant UI if it is
  // not already showing. We don't have enough information here to know what
  // the interaction source is.
  ShowUi(AssistantEntryPoint::kUnspecified);
}

void AssistantUiControllerImpl::OnAssistantControllerConstructed() {
  AssistantInteractionController::Get()->GetModel()->AddObserver(this);
  assistant_controller_->view_delegate()->AddObserver(this);
}

void AssistantUiControllerImpl::OnAssistantControllerDestroying() {
  assistant_controller_->view_delegate()->RemoveObserver(this);
  AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);
}

void AssistantUiControllerImpl::OnOpeningUrl(const GURL& url,
                                             bool in_background,
                                             bool from_server) {
  if (model_.visibility() != AssistantVisibility::kVisible)
    return;

  CloseUi(from_server ? AssistantExitPoint::kNewBrowserTabFromServer
                      : AssistantExitPoint::kNewBrowserTabFromUser);
}

void AssistantUiControllerImpl::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  weak_factory_for_delayed_visibility_changes_.InvalidateWeakPtrs();

  if (new_visibility == AssistantVisibility::kVisible) {
    // Only record the entry point when Assistant UI becomes visible.
    assistant::util::RecordAssistantEntryPoint(entry_point.value());
  }

  if (old_visibility == AssistantVisibility::kVisible) {
    // Only record the exit point when Assistant UI becomes invisible to
    // avoid recording duplicate events (e.g. pressing ESC key).
    assistant::util::RecordAssistantExitPoint(exit_point.value());
  }
}

void AssistantUiControllerImpl::OnOnboardingShown() {
  if (has_shown_onboarding_)
    return;

  has_shown_onboarding_ = true;

  // Update the number of user sessions in which Assistant onboarding was shown.
  pref_service()->SetInteger(prefs::kAssistantNumSessionsWhereOnboardingShown,
                             GetNumberOfSessionsWhereOnboardingShown() + 1);
}

void AssistantUiControllerImpl::OnOverviewModeWillStart() {
  // Close Assistant UI before entering overview mode.
  CloseUi(AssistantExitPoint::kOverviewMode);
}

void AssistantUiControllerImpl::ShowUnboundErrorToast() {
  ShowToast(kUnboundServiceToastId, ToastCatalogName::kAssistantUnboundService,
            IDS_ASH_ASSISTANT_ERROR_GENERIC);
}

}  // namespace ash
