// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"

namespace accessibility {

void RecordPDFOpenedWithA11yFeatureWithPdfOcr() {
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  bool is_pdf_ocr_on = features::IsPdfOcrEnabled();
#else
  bool is_pdf_ocr_on = false;
#endif

  if (accessibility_state_utils::IsScreenReaderEnabled()) {
    UMA_HISTOGRAM_BOOLEAN("Accessibility.PDF.OpenedWithScreenReader.PdfOcr",
                          is_pdf_ocr_on);
  }
  if (accessibility_state_utils::IsSelectToSpeakEnabled()) {
    UMA_HISTOGRAM_BOOLEAN("Accessibility.PDF.OpenedWithSelectToSpeak.PdfOcr",
                          is_pdf_ocr_on);
  }
}

}  // namespace accessibility
