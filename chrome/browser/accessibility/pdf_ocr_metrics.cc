// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/accessibility/pdf_ocr_controller.h"
#include "chrome/browser/accessibility/pdf_ocr_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"

namespace accessibility {

void RecordPDFOpenedWithA11yFeatureWithPdfOcr(
    content::BrowserContext* browser_context) {
  bool is_pdf_ocr_on = false;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  CHECK(browser_context);
  if (features::IsPdfOcrEnabled()) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    is_pdf_ocr_on =
        screen_ai::PdfOcrControllerFactory::GetForProfile(profile)->IsEnabled();
  }
#endif

  if (accessibility_state_utils::IsScreenReaderEnabled()) {
    UMA_HISTOGRAM_BOOLEAN("Accessibility.PDF.OpenedWithScreenReader.PdfOcr2",
                          is_pdf_ocr_on);
  }
  if (accessibility_state_utils::IsSelectToSpeakEnabled()) {
    UMA_HISTOGRAM_BOOLEAN("Accessibility.PDF.OpenedWithSelectToSpeak.PdfOcr2",
                          is_pdf_ocr_on);
  }
}

}  // namespace accessibility
