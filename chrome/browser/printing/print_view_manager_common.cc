// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_common.h"

#include <utility>

#include "build/chromeos_buildflags.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/buildflags/buildflags.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_frame_util.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

namespace printing {

namespace {

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
using PrintViewManagerImpl = PrintViewManager;
#else
using PrintViewManagerImpl = PrintViewManagerBasic;
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

// Pick the right RenderFrameHost based on the WebContents.
content::RenderFrameHost* GetRenderFrameHostToUse(
    content::WebContents* contents) {
#if BUILDFLAG(ENABLE_PDF)
  // Pick the plugin frame host if `contents` is a PDF viewer guest. If using
  // OOPIF PDF viewer, pick the PDF extension frame host.
  content::RenderFrameHost* full_page_pdf_embedder_host =
      chrome_pdf::features::IsOopifPdfEnabled()
          ? pdf_frame_util::FindFullPagePdfExtensionHost(contents)
          : GetFullPagePlugin(contents);
  content::RenderFrameHost* pdf_rfh = pdf_frame_util::FindPdfChildFrame(
      full_page_pdf_embedder_host ? full_page_pdf_embedder_host
                                  : contents->GetPrimaryMainFrame());
  if (pdf_rfh) {
    return pdf_rfh;
  }
#endif  // BUILDFLAG(ENABLE_PDF)
  return GetFrameToPrint(contents);
}

}  // namespace

void StartPrint(
    content::WebContents* contents,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
#endif
    bool print_preview_disabled,
    bool has_selection) {
  content::RenderFrameHost* rfh_to_use = GetRenderFrameHostToUse(contents);
  if (!rfh_to_use)
    return;

  auto* print_view_manager = PrintViewManagerImpl::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh_to_use));
  if (!print_view_manager)
    return;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!print_preview_disabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (print_renderer) {
      print_view_manager->PrintPreviewWithPrintRenderer(
          rfh_to_use, std::move(print_renderer));
      return;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    print_view_manager->PrintPreviewNow(rfh_to_use, has_selection);
    return;
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  print_view_manager->PrintNow(rfh_to_use);
}

void StartBasicPrint(content::WebContents* contents) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  content::RenderFrameHost* rfh_to_use = GetRenderFrameHostToUse(contents);
  if (!rfh_to_use)
    return;

  PrintViewManager* print_view_manager = PrintViewManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh_to_use));
  if (!print_view_manager)
    return;

  print_view_manager->BasicPrint(rfh_to_use);
#endif  // ENABLE_PRINT_PREVIEW
}

void StartPrintNodeUnderContextMenu(content::RenderFrameHost* rfh,
                                    bool print_preview_disabled) {
  auto* print_view_manager = PrintViewManagerImpl::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh));
  if (!print_view_manager) {
    return;
  }

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!print_preview_disabled) {
    print_view_manager->PrintPreviewForNodeUnderContextMenu(rfh);
    return;
  }
#endif

  print_view_manager->PrintNodeUnderContextMenu(rfh);
}

content::RenderFrameHost* GetFrameToPrint(content::WebContents* contents) {
  auto* focused_frame = contents->GetFocusedFrame();
  return (focused_frame && focused_frame->HasSelection())
             ? focused_frame
             : contents->GetPrimaryMainFrame();
}

content::RenderFrameHost* GetFullPagePlugin(content::WebContents* contents) {
  content::RenderFrameHost* full_page_plugin = nullptr;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  contents->ForEachRenderFrameHostWithAction(
      [&full_page_plugin](content::RenderFrameHost* rfh) {
        auto* guest_view =
            extensions::MimeHandlerViewGuest::FromRenderFrameHost(rfh);
        if (guest_view && guest_view->is_full_page_plugin()) {
          DCHECK_EQ(guest_view->GetGuestMainFrame(), rfh);
          full_page_plugin = rfh;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return full_page_plugin;
}

}  // namespace printing
