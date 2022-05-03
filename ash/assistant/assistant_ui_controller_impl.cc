// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_ui_controller_impl.h"

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/histogram_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/system/toast_catalog.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

constexpr char kStylusPromptToastId[] = "stylus_prompt_for_embedded_ui";
constexpr char kUnboundServiceToastId[] =
    "assistant_controller_unbound_service";

void ShowToast(const std::string& id,
               ToastCatalogName catalog_name,
               int message_id) {
  ToastData toast(id, catalog_name, l10n_util::GetStringUTF16(message_id));
  Shell::Get()->toast_manager()->Show(toast);
}

}  // namespace

// AssistantUiControllerImpl ---------------------------------------------------

AssistantUiControllerImpl::AssistantUiControllerImpl(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {
  model_.AddObserver(this);
  assistant_controller_observation_.Observe(AssistantController::Get());
  highlighter_controller_observation_.Observe(
      Shell::Get()->highlighter_controller());
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

void AssistantUiControllerImpl::SetAssistant(
    chromeos::assistant::Assistant* assistant) {
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
      chromeos::assistant::AssistantStatus::NOT_READY) {
    ShowUnboundErrorToast();
    return;
  }

  if (!assistant_) {
    ShowUnboundErrorToast();
    return;
  }

  model_.SetUiMode(AssistantUiMode::kLauncherEmbeddedUi);
  model_.SetVisible(entry_point);
}

absl::optional<base::ScopedClosureRunner> AssistantUiControllerImpl::CloseUi(
    AssistantExitPoint exit_point) {
  if (model_.visibility() != AssistantVisibility::kVisible)
    return absl::nullopt;

  // Set visibility to `kClosing`.
  model_.SetClosing(exit_point);

  // When the return value is destroyed, visibility will be set to `kClosed`
  // provided the visibility change hasn't been invalidated.
  return base::ScopedClosureRunner(base::BindOnce(
      [](const base::WeakPtr<AssistantUiControllerImpl>& weak_ptr,
         chromeos::assistant::AssistantExitPoint exit_point) {
        if (weak_ptr)
          weak_ptr->model_.SetClosed(exit_point);
      },
      weak_factory_for_delayed_visibility_changes_.GetWeakPtr(), exit_point));
}

void AssistantUiControllerImpl::ToggleUi(
    absl::optional<AssistantEntryPoint> entry_point,
    absl::optional<AssistantExitPoint> exit_point) {
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

void AssistantUiControllerImpl::OnInputModalityChanged(
    InputModality input_modality) {
  UpdateUiMode();
}

void AssistantUiControllerImpl::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kActive)
    return;

  // If there is an active interaction, we need to show Assistant UI if it is
  // not already showing. We don't have enough information here to know what
  // the interaction source is.
  ShowUi(AssistantEntryPoint::kUnspecified);

  // We also need to ensure that we're in the appropriate UI mode if we aren't
  // already so that the interaction is visible to the user. Note that we
  // indicate that this UI mode change is occurring due to an interaction so
  // that we won't inadvertently stop the interaction due to the UI mode change.
  UpdateUiMode(AssistantUiMode::kLauncherEmbeddedUi,
               /*due_to_interaction=*/true);
}

void AssistantUiControllerImpl::OnMicStateChanged(MicState mic_state) {
  // When the mic is opened we update the UI mode to ensure that the user is
  // being presented with the main stage. When closing the mic it is appropriate
  // to stay in whatever UI mode we are currently in.
  if (mic_state == MicState::kOpen)
    UpdateUiMode();
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
    absl::optional<AssistantEntryPoint> entry_point,
    absl::optional<AssistantExitPoint> exit_point) {
  weak_factory_for_delayed_visibility_changes_.InvalidateWeakPtrs();

  if (new_visibility == AssistantVisibility::kVisible) {
    // Only record the entry point when Assistant UI becomes visible.
    assistant::util::RecordAssistantEntryPoint(entry_point.value());
  }

  if (old_visibility == AssistantVisibility::kVisible) {
    // Metalayer should not be sticky. Disable when the UI is no longer visible.
    if (exit_point != AssistantExitPoint::kStylus)
      Shell::Get()->highlighter_controller()->AbortSession();

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

void AssistantUiControllerImpl::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  if (state != HighlighterEnabledState::kEnabled)
    return;

  ShowToast(kStylusPromptToastId, ToastCatalogName::kStylusPrompt,
            IDS_ASH_ASSISTANT_PROMPT_STYLUS);
  CloseUi(AssistantExitPoint::kStylus);
}

void AssistantUiControllerImpl::OnOverviewModeWillStart() {
  // Close Assistant UI before entering overview mode.
  CloseUi(AssistantExitPoint::kOverviewMode);
}

void AssistantUiControllerImpl::ShowUnboundErrorToast() {
  ShowToast(kUnboundServiceToastId, ToastCatalogName::kAssistantUnboundService,
            IDS_ASH_ASSISTANT_ERROR_GENERIC);
}

void AssistantUiControllerImpl::UpdateUiMode(
    absl::optional<AssistantUiMode> ui_mode,
    bool due_to_interaction) {
  // If a UI mode is provided, we will use it in lieu of updating UI mode on the
  // basis of interaction/widget visibility state.
  if (ui_mode.has_value()) {
    model_.SetUiMode(ui_mode.value(), due_to_interaction);
    return;
  }

  model_.SetUiMode(AssistantUiMode::kLauncherEmbeddedUi, due_to_interaction);
}

}  // namespace ash
