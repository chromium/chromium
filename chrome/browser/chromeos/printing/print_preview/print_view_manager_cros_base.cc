// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_base.h"

#include "base/values.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "stdint.h"
#include "ui/accessibility/ax_tree_update.h"

namespace chromeos {

PrintViewManagerCrosBase::PrintViewManagerCrosBase(
    content::WebContents* web_contents)
    : PrintManager(web_contents) {}

// TODO(jimmyxgong): Implement stubs.
void PrintViewManagerCrosBase::DidGetPrintedPagesCount(int32_t cookie,
                                                       uint32_t number_pages) {}

void PrintViewManagerCrosBase::DidPrintDocument(
    ::printing::mojom::DidPrintDocumentParamsPtr params,
    DidPrintDocumentCallback callback) {}

void PrintViewManagerCrosBase::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerCrosBase::UpdatePrintSettings(
    base::Value::Dict job_settings,
    UpdatePrintSettingsCallback callback) {}

void PrintViewManagerCrosBase::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {}
#endif

void PrintViewManagerCrosBase::IsPrintingEnabled(
    IsPrintingEnabledCallback callback) {}

void PrintViewManagerCrosBase::ScriptedPrint(
    ::printing::mojom::ScriptedPrintParamsPtr params,
    ScriptedPrintCallback callback) {}

void PrintViewManagerCrosBase::PrintingFailed(
    int32_t cookie,
    ::printing::mojom::PrintFailureReason reason) {}

bool PrintViewManagerCrosBase::PrintNow(content::RenderFrameHost* rfh,
                                        bool has_selection) {
  return false;
}

bool PrintViewManagerCrosBase::IsCrashed() {
  return web_contents()->IsCrashed();
}

}  // namespace chromeos
