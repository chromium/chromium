// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_COMMON_H_
#define CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_COMMON_H_

#include "build/chromeos_buildflags.h"
#include "components/printing/common/print.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "printing/buildflags/buildflags.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace printing {

// Start printing using the appropriate PrintViewManagerBase subclass.
// Optionally provide a printing::mojom::PrintRenderer to render print
// documents.
void StartPrint(
    content::WebContents* web_contents,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
#endif
    bool print_preview_disabled,
    bool has_selection);

// Start printing using the system print dialog.
void StartBasicPrint(content::WebContents* contents);

// Start printing the node under the context menu using the appropriate
// PrintViewManagerBase subclass.
void StartPrintNodeUnderContextMenu(content::RenderFrameHost* rfh,
                                    bool print_preview_disabled);

// If the user has selected text in the currently focused frame, print only that
// frame (this makes print selection work for multiple frames).
content::RenderFrameHost* GetFrameToPrint(content::WebContents* contents);

// If we have a single full-page embedded mime handler view guest, print the
// guest view instead.
content::RenderFrameHost* GetFullPagePlugin(content::WebContents* contents);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_COMMON_H_
