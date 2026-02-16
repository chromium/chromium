// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_CROS_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_CROS_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom-forward.h"

namespace chromeos {

// TODO(crbug.com/484930340): These interfaces probably don't make sense
// anymore. Perhaps even PrintPreviewWebcontentsManager and/or
// PrintPreviewWebcontentsAdapterAsh can be removed.
//
// Client interface implemented by browser. Facilitates requests
// made from ash and implemented by browser.
class PrintPreviewCrosClient {
 public:
  using GeneratePrintPreviewCallback = base::OnceCallback<void(bool)>;
  using HandleDialogClosedCallback = base::OnceCallback<void(bool)>;

  virtual ~PrintPreviewCrosClient() = default;

  // TODO(crbug.com/484928209): This looks like dead code, including
  // ash/public/cpp/print_preview_delegate.h.
  //
  // Start the process of generating a preview for the initiating source. This
  // is done asynchronously. Progress of the preview generation is provided by
  // mojom::PrintPreviewUI. `settings` is the print settings provided by
  // the print preview UI.
  virtual void GeneratePrintPreview(const base::UnguessableToken& token,
                                    crosapi::mojom::PrintSettingsPtr settings,
                                    GeneratePrintPreviewCallback callback) = 0;

  // Handle when the print preview dialog is closed by navigation. For
  // example, closing dialog via Exit navigation button.
  virtual void HandleDialogClosed(const base::UnguessableToken& token,
                                  HandleDialogClosedCallback callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_CROS_CLIENT_H_
