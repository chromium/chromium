// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_MANAGER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

// Manages a 1:1 relationship between a printing source's webcontents and a
// base::UnguessableToken. Each token represent a webcontent and is used as a
// proxy for determining which print preview is relevant.
// Communicates to ash via crosapi.
class PrintPreviewWebcontentsManager
    : public crosapi::mojom::PrintPreviewCrosClient {
 public:
  PrintPreviewWebcontentsManager();
  PrintPreviewWebcontentsManager(const PrintPreviewWebcontentsManager&) =
      delete;
  PrintPreviewWebcontentsManager& operator=(
      const PrintPreviewWebcontentsManager&) = delete;
  ~PrintPreviewWebcontentsManager() override;

  static PrintPreviewWebcontentsManager* Get();
  static void SetInstanceForTesting(PrintPreviewWebcontentsManager* manager);
  static void ResetInstanceForTesting();

  void Initialize();

  // Establishes new mappings of webcontents and token, then requests a new
  // print preview dialog to appear.
  void RequestPrintPreview(
      const base::UnguessableToken& token,
      content::WebContents* webcontents,
      ::printing::mojom::RequestPrintPreviewParamsPtr params);

  // Handles removing the webcontents mapping and informing the ash client
  // of the removed webcontent. This can happen if the initiating source
  // (e.g. tab) closes/crashes.
  void PrintPreviewDone(const base::UnguessableToken& token);

  // crosapi::mojom::PrintPreviewCrosClient:
  void GeneratePrintPreview(const base::UnguessableToken& token,
                            crosapi::mojom::PrintSettingsPtr settings,
                            GeneratePrintPreviewCallback callback) override;
  // Handles ash -> chrome requests when the print dialog is closed.
  void HandleDialogClosed(const base::UnguessableToken& token,
                          HandleDialogClosedCallback callback) override;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void ResetRemoteForTesting();
  void BindPrintPreviewCrosDelegateForTesting(
      mojo::PendingRemote<crosapi::mojom::PrintPreviewCrosDelegate>
          pending_remote);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  static void SetPrintPreviewCrosDelegateForTesting(
      crosapi::mojom::PrintPreviewCrosDelegate* delegate);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  friend class MockPrintPreviewWebcontentsManager;
  friend class PrintPreviewWebContentsManagerBrowserTest;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  mojo::Remote<crosapi::mojom::PrintPreviewCrosDelegate> remote_;
  mojo::Receiver<crosapi::mojom::PrintPreviewCrosClient> receiver_{this};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  void OnRequestPrintPreviewCallback(bool success);
  void OnPrintPreviewDoneCallback(bool success);

  // Remove the entry from lookup maps keyed by `token`.
  // Returns the webcontents tied to `token` if removal was successful.
  content::WebContents* RemoveTokenMapping(const base::UnguessableToken& token);

  // Mapping a unique ID to its webcontents.
  std::map<base::UnguessableToken,
           raw_ptr<content::WebContents, CtnExperimental>>
      token_to_webcontents_;

  base::WeakPtrFactory<PrintPreviewWebcontentsManager> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_MANAGER_H_
