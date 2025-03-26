// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_AX_MAIN_NODE_ANNOTATOR_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_AX_MAIN_NODE_ANNOTATOR_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

#include "ui/accessibility/platform/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform.h"

class Profile;

namespace content {
class ScopedAccessibilityMode;
}  // namespace content

namespace screen_ai {

class AXMainNodeAnnotatorControllerFactory;

// Manages the feature that identifies the main landmark in a web page. Observes
// changes in the per-profile preference and updates the accessibility mode of
// WebContents when it changes, provided its feature flag is enabled.
class AXMainNodeAnnotatorController : public KeyedService,
                                      public ScreenAIInstallState::Observer,
                                      public ui::AXModeObserver {
 public:
  explicit AXMainNodeAnnotatorController(Profile* profile);
  AXMainNodeAnnotatorController(const AXMainNodeAnnotatorController&) = delete;
  AXMainNodeAnnotatorController& operator=(
      const AXMainNodeAnnotatorController&) = delete;
  ~AXMainNodeAnnotatorController() override;

  // Return true if main node annotations are enabled for the profile.
  bool IsEnabled() const;

  // ScreenAIInstallState::Observer:
  void StateChanged(ScreenAIInstallState::State state) override;

  void Activate();

  // ui::AXModeObserver:
  void OnAssistiveTechChanged(ui::AssistiveTech assistive_tech) override;

  void set_service_ready_for_testing() { service_ready_ = true; }
  void complete_service_intialization_for_testing() {
    MainNodeExtractionServiceInitializationCallback(true);
  }

 private:
  friend class AXMainNodeAnnotatorControllerFactory;

  // Receives the result of main node extraction service initialization.
  void MainNodeExtractionServiceInitializationCallback(bool successful);

  // Handles a change to the activation state.
  void OnActivationChanged();

  // Handles a change to the user preference.
  void OnAXMainNodeAnnotationsEnabledChanged();

  // AXMainNodeAnnotatorController will be created via
  // AXMainNodeAnnotatorControllerFactory on this profile and then destroyed
  // before the profile gets destroyed.
  raw_ptr<Profile> profile_;

  // Observes the presence of a screen reader.
  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      ax_mode_observation_{this};

  // Observes changes in Screen AI component download and readiness state.
  base::ScopedObservation<ScreenAIInstallState, ScreenAIInstallState::Observer>
      component_ready_observer_{this};

  PrefChangeRegistrar pref_change_registrar_;

  // Enables the kAnnotateMainNode accessibility mode flag for all tabs
  // associated with the controller's profile.
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

  // True when main node extraction service is initialized and ready to use.
  bool service_ready_ = false;

  // Main node extraction initialization has started, but is not finished yet.
  bool waiting_for_service_initialization_ = false;

  bool activated_ = false;

  base::WeakPtrFactory<AXMainNodeAnnotatorController> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_ACCESSIBILITY_AX_MAIN_NODE_ANNOTATOR_CONTROLLER_H_
