// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_CROS_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_CROS_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "components/printing/common/print.mojom-forward.h"

namespace chromeos {

// TODO(crbug.com/484930340): These interfaces probably don't make sense
// anymore. Perhaps even PrintPreviewWebcontentsManager and/or
// PrintPreviewWebcontentsAdapterAsh can be removed.
//
// Delegate implemented by ash. Informs ash of webcontent-related events from
// browser (backed by ash) to ash.
class PrintPreviewCrosDelegate {
 public:
  using RequestPrintPreviewCallback = base::OnceCallback<void(bool)>;
  using PrintPreviewDoneCallback = base::OnceCallback<void(bool)>;

  virtual ~PrintPreviewCrosDelegate() = default;

  // Called when a webcontent requests to open a new instance of print preview.
  virtual void RequestPrintPreview(
      const base::UnguessableToken& token,
      ::printing::mojom::RequestPrintPreviewParamsPtr params,
      RequestPrintPreviewCallback callback) = 0;

  // Called when a webcontent is done (canceled or completed) and therefore its
  // print preview must be closed.
  virtual void PrintPreviewDone(const base::UnguessableToken& token,
                                PrintPreviewDoneCallback callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_CROS_DELEGATE_H_
