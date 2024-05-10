// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_common.h"

#include <utility>

#include "build/chromeos_buildflags.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros.h"
#else
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

namespace chromeos::printing {

namespace {

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
using PrintViewManagerImpl = PrintViewManagerCros;
#else
using PrintViewManagerImpl = PrintViewManagerCrosBasic;
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

// Pick the right RenderFrameHost based on the WebContents.
content::RenderFrameHost* GetRenderFrameHostToUse(
    content::WebContents* contents) {
  // TODO(jimmyxgong): Add in PDF plugin frame.
  auto* focused_frame = contents->GetFocusedFrame();
  return (focused_frame && focused_frame->HasSelection())
             ? focused_frame
             : contents->GetPrimaryMainFrame();
}

}  // namespace

void StartPrint(content::WebContents* contents,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                mojo::PendingAssociatedRemote<::printing::mojom::PrintRenderer>
                    print_renderer,
#endif
                bool print_preview_disabled,
                bool has_selection) {
  content::RenderFrameHost* rfh_to_use = GetRenderFrameHostToUse(contents);
  if (!rfh_to_use) {
    return;
  }

  auto* print_view_manager = PrintViewManagerImpl::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh_to_use));
  if (!print_view_manager) {
    return;
  }

  // TODO(jimmyxgong): Handle print preview disabled state.
  print_view_manager->PrintPreviewNow(rfh_to_use, has_selection);
}

}  // namespace chromeos::printing
