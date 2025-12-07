// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "ui/accessibility/platform/ax_platform.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace accessibility {

void RecordPDFOpenedWithA11yFeatureWithPdfOcr() {
#if BUILDFLAG(IS_ANDROID)
  bool is_pdf_ocr_on = false;
#else
  bool is_pdf_ocr_on = true;
#endif

  if (ui::AXPlatform::GetInstance().IsScreenReaderActive()) {
    UMA_HISTOGRAM_BOOLEAN("Accessibility.PDF.OpenedWithScreenReader.PdfOcr",
                          is_pdf_ocr_on);
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (ash::AccessibilityManager::Get()->IsSelectToSpeakEnabled()) {
    UMA_HISTOGRAM_BOOLEAN("Accessibility.PDF.OpenedWithSelectToSpeak.PdfOcr",
                          is_pdf_ocr_on);
  }
#endif
}

}  // namespace accessibility
