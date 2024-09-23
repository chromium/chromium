// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/callback_list.h"
#else
#include "base/scoped_observation.h"
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
// Observes changes in assitive technologies and updates the accessibility
// mode of WebContents when the feature is needed.
class PdfOcrController : public KeyedService
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

  // Return all PDF-related WebContentses associated with a given Profile.
  static std::vector<content::WebContents*> GetAllPdfWebContentsForTesting(
      Profile* profile);

  // Return true if PDF OCR is enabled for the profile.F
  bool IsEnabled() const;

  void set_ocr_ready_for_testing() { ocr_service_ready_ = true; }

  void set_initialization_retry_wait_for_testing(const base::TimeDelta& wait) {
    initialization_retry_wait_ = wait;
  }

  void Activate();

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

  // Sends an initialization request to ScreenAIServiceRouter if one is not
  // pending.
  void InitializeService();

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

  // Enables the kPDFOcr accessibility mode flag for all tabs associated
  // with the controller's profile.
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

  // True when OCR service is initialized and ready to use.
  bool ocr_service_ready_ = false;

  // OCR initialization has started, but is not finished yet.
  bool waiting_for_ocr_service_initialization_ = false;

  // Number of times initialization is retried.
  uint32_t initialization_retries_ = 0;

  base::TimeDelta initialization_retry_wait_;

  base::WeakPtrFactory<PdfOcrController> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_H_
