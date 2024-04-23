// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_preview/print_preview_dialog_controller_cros.h"

#include "base/unguessable_token.h"
#include "components/printing/common/print.mojom.h"

namespace ash {
void PrintPreviewDialogControllerCros::CreatePrintPreview(
    base::UnguessableToken token,
    printing::mojom::RequestPrintPreviewParams params) {}

void PrintPreviewDialogControllerCros::RemovePrintPreview(
    base::UnguessableToken token) {}

}  // namespace ash
