// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/ax_main_node_annotator_controller.h"

#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif

namespace {

// Invoke screen reader alert to notify the user of the state.
void AnnounceToScreenReader(const int message_id) {
  const Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser) {
    VLOG(2) << "Browser is not ready to announce";
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    VLOG(2) << "Browser is not ready to announce";
    return;
  }

  browser_view->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(message_id));
}

}  // namespace

namespace screen_ai {

AXMainNodeAnnotatorController::AXMainNodeAnnotatorController(Profile* profile)
    : profile_(profile) {
  // Initialize an observer for changes of AX Main Node Annotation pref.
  DCHECK(profile_);
  VLOG(2) << "Init AXMainNodeAnnotatorController";
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kAccessibilityMainNodeAnnotationsEnabled,
      base::BindRepeating(
          &AXMainNodeAnnotatorController::OnAXMainNodeAnnotationsEnabledChanged,
          weak_ptr_factory_.GetWeakPtr()));

  // Register for changes to screenreader/spoken feedback.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (auto* const accessibility_manager = ash::AccessibilityManager::Get();
      accessibility_manager) {
    // Unretained is safe because `this` owns the subscription.
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &AXMainNodeAnnotatorController::OnAccessibilityStatusEvent,
            base::Unretained(this)));
  }
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  activated_ = accessibility_state_utils::IsScreenReaderEnabled();
  OnActivationChanged();
}

AXMainNodeAnnotatorController::~AXMainNodeAnnotatorController() = default;

bool AXMainNodeAnnotatorController::IsEnabled() const {
  return scoped_accessibility_mode_ != nullptr;
}

void AXMainNodeAnnotatorController::OnAXMainNodeAnnotationsEnabledChanged() {
  const auto& pref_value = profile_->GetPrefs()->GetValue(
      prefs::kAccessibilityMainNodeAnnotationsEnabled);
  VLOG(2) << "AXMainNode annotations enabled changed: " << pref_value.GetBool();

  OnActivationChanged();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AXMainNodeAnnotatorController::OnAccessibilityStatusEvent(
    const ash::AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
      ash::AccessibilityNotificationType::kToggleSpokenFeedback) {
    activated_ = accessibility_state_utils::IsScreenReaderEnabled();
    OnActivationChanged();
  }
}
#endif  // BUIDLFLAG(IS_CHROMEOS_ASH)

void AXMainNodeAnnotatorController::OnActivationChanged() {
  const bool is_activated =
      activated_ && profile_->GetPrefs()->GetBoolean(
                        prefs::kAccessibilityMainNodeAnnotationsEnabled);

  if (is_activated == IsEnabled()) {
    return;  // No change in activation.
  }

  if (is_activated) {
    if (!service_ready_) {
      // Avoid repeated requests.
      if (waiting_for_service_initialization_) {
        return;
      }
      waiting_for_service_initialization_ = true;

      if (ScreenAIInstallState::GetInstance()->get_state() !=
              ScreenAIInstallState::State::kDownloaded &&
          !component_ready_observer_.IsObserving()) {
        // Start observing ScreenAIInstallState to report it to user.
        component_ready_observer_.Observe(ScreenAIInstallState::GetInstance());
      }

      screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
          ->GetServiceStateAsync(
              ScreenAIServiceRouter::Service::kMainContentExtraction,
              base::BindOnce(
                  &AXMainNodeAnnotatorController::
                      MainNodeExtractionServiceInitializationCallback,
                  weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    // This will send the `kAnnotateMainNode` flag to all WebContents.
    scoped_accessibility_mode_ =
        content::BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForBrowserContext(profile_,
                                                ui::AXMode::kAnnotateMainNode);
  } else {
    scoped_accessibility_mode_.reset();
  }
}

void AXMainNodeAnnotatorController::
    MainNodeExtractionServiceInitializationCallback(bool successful) {
  waiting_for_service_initialization_ = false;
  service_ready_ = successful;
  if (successful) {
    OnActivationChanged();
  } else {
    // Call `StateChanged` to announce the state to user.
    StateChanged(ScreenAIInstallState::State::kDownloadFailed);
  }

  // No more need for observing Screen AI state changes.
  component_ready_observer_.Reset();
}

void AXMainNodeAnnotatorController::StateChanged(
    ScreenAIInstallState::State state) {
  switch (state) {
    case ScreenAIInstallState::State::kNotDownloaded:
      break;

    case ScreenAIInstallState::State::kDownloading:
      AnnounceToScreenReader(IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_DOWNLOADING);
      break;

    case ScreenAIInstallState::State::kDownloadFailed:
      AnnounceToScreenReader(IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_DOWNLOAD_ERROR);
      // Update the main node annotations pref to be false to toggle off the
      // button.
      profile_->GetPrefs()->SetBoolean(
          prefs::kAccessibilityMainNodeAnnotationsEnabled, false);
      break;

    case ScreenAIInstallState::State::kDownloaded:
      AnnounceToScreenReader(
          IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_DOWNLOAD_COMPLETE);
      break;
  }
}

void AXMainNodeAnnotatorController::Activate() {
  activated_ = true;
  OnActivationChanged();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void AXMainNodeAnnotatorController::OnAXModeAdded(ui::AXMode mode) {
  if (mode.has_mode(ui::AXMode::kScreenReader)) {
    activated_ = true;
    OnActivationChanged();
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace screen_ai
