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

#if BUILDFLAG(IS_ANDROID)
#include "base/files/scoped_file.h"
#endif

namespace printing {

class PrinterQuery;
#if BUILDFLAG(IS_ANDROID)
class PrintSettings;
#endif

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
  // Prints the document with pre-configured settings and direct output FD.
  bool PrintNowAndroid(content::RenderFrameHost* rfh,
                       std::unique_ptr<PrintSettings> settings,
                       base::ScopedFD file_descriptor);

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
  void DidGetPrintedPagesCount(int32_t cookie, uint32_t number_pages) override;
  void DidPrintDocument(mojom::DidPrintDocumentParamsPtr params,
                        DidPrintDocumentCallback callback) override;
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override;
  void ScriptedPrint(mojom::ScriptedPrintParamsPtr params,
                     ScriptedPrintCallback callback) override;
  void PrintingFailed(int32_t cookie,
                      mojom::PrintFailureReason reason) override;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  explicit PrintViewManagerBasic(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrintViewManagerBasic>;

#if BUILDFLAG(IS_ANDROID)
  void OnSetupScriptedPrintAndroidDone(
      SetupScriptedPrintAndroidCallback callback,
      std::unique_ptr<PrinterQuery> printer_query);

  // Whether `document_cookie` still names the job that owns `settings_`. A
  // composite or write that completes after FinishPrint() must not report
  // into a job started after it.
  bool IsCurrentJob(int32_t document_cookie) const;

  void OnDidPrintDocumentWritingDone(
      int32_t document_cookie,
      DidPrintDocumentCallback did_print_document_cb,
      uint32_t page_count);

  void OnDidPrintDocumentDone(DidPrintDocumentCallback callback,
                              bool succeeded);

  void OnCompositeDocumentDoneAndroid(int32_t document_cookie,
                                      base::ScopedFD print_fd,
                                      uint32_t page_count,
                                      DidPrintDocumentCallback callback,
                                      mojom::PrintCompositor::Status status,
                                      base::ReadOnlySharedMemoryRegion region);

  void WriteDocumentToFd(
      int32_t document_cookie,
      base::ScopedFD print_fd,
      uint32_t page_count,
      scoped_refptr<base::RefCountedSharedMemoryMapping> data,
      DidPrintDocumentCallback callback);

  std::unique_ptr<PrintSettings> settings_;
  base::ScopedFD fd_;
#endif

  WEB_CONTENTS_USER_DATA_KEY_DECL();

#if BUILDFLAG(IS_ANDROID)
  base::WeakPtrFactory<PrintViewManagerBasic> weak_ptr_factory_{this};
#endif
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASIC_H_
