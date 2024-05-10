// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_BASE_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_BASE_H_

#include "base/values.h"
#include "components/printing/browser/print_manager.h"
#include "stdint.h"
#include "ui/accessibility/ax_tree_update.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace chromeos {

// Base class for managing the print commands for a WebContents.
class PrintViewManagerCrosBase : public ::printing::PrintManager {
 public:
  PrintViewManagerCrosBase(const PrintViewManagerCrosBase&) = delete;
  PrintViewManagerCrosBase& operator=(const PrintViewManagerCrosBase&) = delete;

  ~PrintViewManagerCrosBase() override = default;

  // mojom::PrintManagerHost:
  void DidGetPrintedPagesCount(int32_t cookie, uint32_t number_pages) override;
  void DidPrintDocument(::printing::mojom::DidPrintDocumentParamsPtr params,
                        DidPrintDocumentCallback callback) override;
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void UpdatePrintSettings(base::Value::Dict job_settings,
                           UpdatePrintSettingsCallback callback) override;
  void SetAccessibilityTree(
      int32_t cookie,
      const ui::AXTreeUpdate& accessibility_tree) override;
#endif
  void IsPrintingEnabled(IsPrintingEnabledCallback callback) override;
  void ScriptedPrint(::printing::mojom::ScriptedPrintParamsPtr params,
                     ScriptedPrintCallback callback) override;
  void PrintingFailed(int32_t cookie,
                      ::printing::mojom::PrintFailureReason reason) override;

  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function. Returns false if printing is impossible at the moment.
  virtual bool PrintNow(content::RenderFrameHost* rfh, bool has_selection);

 protected:
  explicit PrintViewManagerCrosBase(content::WebContents* web_contents);

  // Return true if the webcontent is no longer available due to a crash.
  bool IsCrashed();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_BASE_H_
