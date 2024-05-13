// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_ADAPTER_ASH_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_ADAPTER_ASH_H_

#include "ash/public/cpp/print_preview_delegate.h"
#include "base/unguessable_token.h"

namespace ash::printing {

// Implements the PrintPreviewDelegate and is the adapter to facilitate calls
// from ash to chrome browser. It uses crosapi to handle cross process
// communication.
class PrintPreviewWebcontentsAdapterAsh : public PrintPreviewDelegate {
 public:
  PrintPreviewWebcontentsAdapterAsh() = default;
  PrintPreviewWebcontentsAdapterAsh(const PrintPreviewWebcontentsAdapterAsh&) =
      delete;
  PrintPreviewWebcontentsAdapterAsh& operator=(
      const PrintPreviewWebcontentsAdapterAsh&) = delete;
  ~PrintPreviewWebcontentsAdapterAsh() override = default;

  // PrintPreviewDelegate::
  void StartGetPreview(base::UnguessableToken token) override;
};

}  // namespace ash::printing

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_ADAPTER_ASH_H_
