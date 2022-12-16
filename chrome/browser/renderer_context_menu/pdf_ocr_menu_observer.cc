// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/pdf_ocr_menu_observer.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
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

}  // namespace

PdfOcrMenuObserver::PdfOcrMenuObserver(RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {}

PdfOcrMenuObserver::~PdfOcrMenuObserver() = default;

void PdfOcrMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile != nullptr);
  if (ShouldShowPdfOcrMenuItem()) {
    proxy_->AddPdfOcrMenuItem(profile->GetPrefs()->GetBoolean(
        prefs::kAccessibilityPdfOcrAlwaysActive));
  }
}

bool PdfOcrMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_PDF_OCR ||
         command_id == IDC_CONTENT_CONTEXT_PDF_OCR_ALWAYS ||
         command_id == IDC_CONTENT_CONTEXT_PDF_OCR_ONCE;
}

bool PdfOcrMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile != nullptr);
  if (command_id == IDC_CONTENT_CONTEXT_PDF_OCR ||
      command_id == IDC_CONTENT_CONTEXT_PDF_OCR_ALWAYS ||
      command_id == IDC_CONTENT_CONTEXT_PDF_OCR_ONCE) {
    return profile->GetPrefs()->GetBoolean(
        prefs::kAccessibilityPdfOcrAlwaysActive);
  }
  return false;
}

bool PdfOcrMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  if (command_id == IDC_CONTENT_CONTEXT_PDF_OCR ||
      command_id == IDC_CONTENT_CONTEXT_PDF_OCR_ALWAYS ||
      command_id == IDC_CONTENT_CONTEXT_PDF_OCR_ONCE) {
    return ShouldShowPdfOcrMenuItem();
  }
  return false;
}

void PdfOcrMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile != nullptr);
  bool is_always_active =
      profile->GetPrefs()->GetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive);
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_PDF_OCR:
      // If the user has selected to make PDF OCR always active, we directly
      // update the profile and change it to the original menu item when the
      // user disables this item.
      DCHECK(is_always_active);
      profile->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                      false);
      // TODO(crbug.com/1393069): Stop the PDF OCR if running.
      NOTIMPLEMENTED() << "Need to stop PDF OCR accordingly";
      break;
    case IDC_CONTENT_CONTEXT_PDF_OCR_ALWAYS:
      // When a user choose "Always" to run the PDF OCR, we save this
      // preference and change this item to a check item in the context menu.
      if (!is_always_active) {
        profile->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                        true);
        // TODO(crbug.com/1393069): Start the PDF OCR if true is set.
        NOTIMPLEMENTED() << "Need to start PDF OCR accordingly";
      }
      break;
    case IDC_CONTENT_CONTEXT_PDF_OCR_ONCE:
      // TODO(crbug.com/1393069): Run PDF OCR once to convert image to text.
      NOTIMPLEMENTED() << "Need to run PDF OCR once to convert image to text";
      break;
    default:
      NOTREACHED();
  }
}
