// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASIC_H_
#define CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASIC_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "content/public/browser/web_contents_user_data.h"

namespace printing {

class PrinterQuery;

// Manages the print commands for a WebContents - basic version.
class PrintViewManagerBasic
    : public PrintViewManagerBase,
      public content::WebContentsUserData<PrintViewManagerBasic> {
 public:
  PrintViewManagerBasic(const PrintViewManagerBasic&) = delete;
  PrintViewManagerBasic& operator=(const PrintViewManagerBasic&) = delete;

  ~PrintViewManagerBasic() override;

  static void BindPrintManagerHost(
      mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
      content::RenderFrameHost* rfh);

#if BUILDFLAG(IS_ANDROID)
  // Asks `rfh` to dispatch beforeprint early, before the Android print
  // framework requests the document content. Returns true if the IPC was sent,
  // or false if `rfh` cannot receive the message.
  bool InitiatePrint(content::RenderFrameHost* rfh);

  // Finishes the print session for `rfh`.
  void FinishPrint(content::RenderFrameHost* rfh);

  // printing::PrintManager:
  void SetupScriptedPrintAndroid(
      SetupScriptedPrintAndroidCallback callback) override;
  void PdfWritingDone(int page_count) override;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  explicit PrintViewManagerBasic(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrintViewManagerBasic>;

#if BUILDFLAG(IS_ANDROID)
  void OnSetupScriptedPrintAndroidDone(
      SetupScriptedPrintAndroidCallback callback,
      std::unique_ptr<PrinterQuery> printer_query);
#endif

  WEB_CONTENTS_USER_DATA_KEY_DECL();

#if BUILDFLAG(IS_ANDROID)
  base::WeakPtrFactory<PrintViewManagerBasic> weak_ptr_factory_{this};
#endif
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASIC_H_
