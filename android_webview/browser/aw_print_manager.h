// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PRINT_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PRINT_MANAGER_H_

#include <memory>

#include "components/printing/browser/print_manager.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/browser/web_contents_user_data.h"
#include "printing/print_settings.h"

namespace android_webview {
// Lifetime: WebView
class AwPrintManager : public printing::PrintManager,
    public content::WebContentsUserData<AwPrintManager> {
 public:
  AwPrintManager(const AwPrintManager&) = delete;
  AwPrintManager& operator=(const AwPrintManager&) = delete;

  ~AwPrintManager() override;

  static void BindPrintManagerHost(
      mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost>
          receiver,
      content::RenderFrameHost* rfh);

  // printing::PrintManager:
  void PdfWritingDone(int page_count) override;

  bool PrintNow();

  // Updates the parameters for printing.
  void UpdateParam(std::unique_ptr<printing::PrintSettings> settings,
                   int file_descriptor,
                   PdfWritingDoneCallback callback);

 private:
  friend class content::WebContentsUserData<AwPrintManager>;

  explicit AwPrintManager(content::WebContents* contents);

  // mojom::PrintManagerHost:
  void DidPrintDocument(printing::mojom::DidPrintDocumentParamsPtr params,
                        DidPrintDocumentCallback callback) override;
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override;
  void ScriptedPrint(printing::mojom::ScriptedPrintParamsPtr params,
                     ScriptedPrintCallback callback) override;

  static void OnDidPrintDocumentWritingDone(
      const PdfWritingDoneCallback& callback,
      DidPrintDocumentCallback did_print_document_cb,
      uint32_t page_count);

  std::unique_ptr<printing::PrintSettings> settings_;

  // The file descriptor into which the PDF of the document will be written.
  int fd_ = -1;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PRINT_MANAGER_H_
