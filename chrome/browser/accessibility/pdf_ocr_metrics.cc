// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace accessibility {

void RecordPDFOpenedWithA11yFeatureWithPdfOcr(
    content::BrowserContext* browser_context) {
  CHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile);
  const PrefService* prefs = profile->GetPrefs();
  CHECK(prefs);
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kAccessibilityPdfOcrAlwaysActive);
  if (!pref) {
    return;
  }
  CHECK(pref->GetValue()->is_bool());

  bool is_pdf_ocr_on = pref->GetValue()->GetBool();
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
