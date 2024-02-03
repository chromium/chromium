// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/callback_list.h"
#else
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform.h"
#endif

class Profile;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
struct AccessibilityStatusEventDetails;
}
#endif

namespace content {
class ScopedAccessibilityMode;
class WebContents;
}

namespace screen_ai {

class PdfOcrControllerFactory;

// Manages the PDF OCR feature that extracts text from an inaccessible PDF.
// Observes changes in the per-profile preference and updates the accessibility
// mode of WebContents when it changes, provided its feature flag is enabled.
class PdfOcrController : public KeyedService,
                         public ScreenAIInstallState::Observer
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    ,
                         public ui::AXModeObserver
#endif
{
 public:
  explicit PdfOcrController(Profile* profile);
  PdfOcrController(const PdfOcrController&) = delete;
  PdfOcrController& operator=(const PdfOcrController&) = delete;
  ~PdfOcrController() override;

  // Return all PDF-related WebContentses associated with the PDF Viewer
  // Mimehandlers in a given Profile for testing.
  static std::vector<content::WebContents*> GetAllPdfWebContentsesForTesting(
      Profile* profile);

  // Return true if PDF OCR is enabled for the profile.F
  bool IsEnabled() const;

  // ScreenAIInstallState::Observer:
  void StateChanged(ScreenAIInstallState::State state) override;

  void set_ocr_ready_for_testing() { ocr_service_ready_ = true; }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // ui::AXModeObserver:
  void OnAXModeAdded(ui::AXMode mode) override;
#endif

 private:
  friend class PdfOcrControllerFactory;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnAccessibilityStatusEvent(
      const ash::AccessibilityStatusEventDetails& details);
#endif

  // Receives the result of OCR service initialization.
  void OCRServiceInitializationCallback(bool successful);

  // Handles a change to the activation state.
  void OnActivationChanged();

  // Handles a change to the user preference.
  void OnPdfOcrAlwaysActiveChanged();

  // Sends Pdf Ocr Always Active state to all relevant WebContents.
  void SendPdfOcrAlwaysActiveToAll(bool is_always_active);

  // PdfOcrController will be created via PdfOcrControllerFactory on this
  // profile and then destroyed before the profile gets destroyed.
  raw_ptr<Profile> profile_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Observes spoken feedback and select to speak.
  base::CallbackListSubscription accessibility_status_subscription_;
#else
  // Observes the presence of a screen reader.
  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      ax_mode_observation_{this};
#endif

  // Observes changes in Screen AI component download and readiness state.
  base::ScopedObservation<ScreenAIInstallState, ScreenAIInstallState::Observer>
      component_ready_observer_{this};

  PrefChangeRegistrar pref_change_registrar_;

  // Enables the kPDFOcr accessibility mode flag for all tabs associated
  // with the controller's profile.
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

  // True when OCR service is initialized and ready to use.
  bool ocr_service_ready_ = false;

  // OCR initialization has started, but is not finished yet.
  bool waiting_for_ocr_service_initialization_ = false;

  base::WeakPtrFactory<PdfOcrController> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_H_
