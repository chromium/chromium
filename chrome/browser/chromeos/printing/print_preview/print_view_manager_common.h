// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_COMMON_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_COMMON_H_

#include "build/chromeos_buildflags.h"
#include "components/printing/common/print.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "printing/buildflags/buildflags.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromeos::printing {
// Start printing using the appropriate PrintViewManagerBase subclass.
// Optionally provide a printing::mojom::PrintRenderer to render print
// documents.
void StartPrint(content::WebContents* web_contents,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                mojo::PendingAssociatedRemote<::printing::mojom::PrintRenderer>
                    print_renderer,
#endif
                bool print_preview_disabled,
                bool has_selection);
}  // namespace chromeos::printing

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_COMMON_H_
