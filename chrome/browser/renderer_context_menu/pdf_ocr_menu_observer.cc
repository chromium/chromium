// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/pdf_ocr_menu_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/accessibility/pdf_ocr_controller.h"
#include "chrome/browser/accessibility/pdf_ocr_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/accessibility/accessibility_features.h"

using content::BrowserThread;

namespace {

// Whether the PDF OCR menu item should be shown in the menu. It now depends on
// whether a screen reader is running.
bool ShouldShowPdfOcrMenuItem() {
  return accessibility_state_utils::IsScreenReaderEnabled() &&
         features::IsPdfOcrEnabled();
}

void RecordUserSelection(PdfOcrUserSelection user_selection) {
  base::UmaHistogramEnumeration("Accessibility.PdfOcr.UserSelection",
                                user_selection);
}

}  // namespace

PdfOcrMenuObserver::PdfOcrMenuObserver(RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {}

PdfOcrMenuObserver::~PdfOcrMenuObserver() = default;

void PdfOcrMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile != nullptr);
  if (ShouldShowPdfOcrMenuItem()) {
    proxy_->AddPdfOcrMenuItem();
  }
}

bool PdfOcrMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_PDF_OCR;
}

bool PdfOcrMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile != nullptr);
  return (command_id == IDC_CONTENT_CONTEXT_PDF_OCR
              ? profile->GetPrefs()->GetBoolean(
                    prefs::kAccessibilityPdfOcrAlwaysActive)
              : false);
}

bool PdfOcrMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  return (command_id == IDC_CONTENT_CONTEXT_PDF_OCR ? ShouldShowPdfOcrMenuItem()
                                                    : false);
}

void PdfOcrMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile != nullptr);
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_PDF_OCR:
      // If PDF OCR is already on, turn off PDF OCR. Otherwise, turn on PDF OCR.
      if (profile->GetPrefs()->GetBoolean(
              prefs::kAccessibilityPdfOcrAlwaysActive)) {
        VLOG(2) << "Turning off PDF OCR from the context menu";
        profile->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                        false);
        RecordUserSelection(PdfOcrUserSelection::kTurnOffFromContextMenu);
      } else {
        VLOG(2) << "Turning on PDF OCR from the context menu";
        profile->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                        true);
        RecordUserSelection(PdfOcrUserSelection::kTurnOnAlwaysFromContextMenu);
      }
      break;
    default:
      NOTREACHED();
  }
}
