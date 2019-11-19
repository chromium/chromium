// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_common.h"

#include "base/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

namespace printing {

namespace {
#if BUILDFLAG(ENABLE_EXTENSIONS)
// Stores |guest_contents| in |result| and returns true if |guest_contents| is a
// full page MimeHandlerViewGuest plugin. Otherwise, returns false.
bool StoreFullPagePlugin(content::WebContents** result,
                         content::WebContents* guest_contents) {
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(guest_contents);
  if (guest_view && guest_view->is_full_page_plugin()) {
    *result = guest_contents;
    return true;
  }
  return false;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Pick the right RenderFrameHost based on the WebContentses.
content::RenderFrameHost* GetRenderFrameHostToUse(
    content::WebContents* original_contents,
    content::WebContents* contents_to_use) {
  if (original_contents != contents_to_use)
    return contents_to_use->GetMainFrame();
  return GetFrameToPrint(contents_to_use);
}

}  // namespace

void StartPrint(content::WebContents* contents,
                mojom::PrintRendererAssociatedPtrInfo print_renderer,
                bool print_preview_disabled,
                bool has_selection) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  using PrintViewManagerImpl = PrintViewManager;
#else
  using PrintViewManagerImpl = PrintViewManagerBasic;
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  content::WebContents* contents_to_use = GetWebContentsToUse(contents);
  auto* print_view_manager =
      PrintViewManagerImpl::FromWebContents(contents_to_use);
  if (!print_view_manager)
    return;

  content::RenderFrameHost* rfh_to_use =
      GetRenderFrameHostToUse(contents, contents_to_use);
  if (!rfh_to_use)
    return;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!print_preview_disabled) {
    if (print_renderer) {
      print_view_manager->PrintPreviewWithPrintRenderer(
          rfh_to_use, std::move(print_renderer));
    } else {
      print_view_manager->PrintPreviewNow(rfh_to_use, has_selection);
    }
    return;
  }
#endif  // ENABLE_PRINT_PREVIEW

  print_view_manager->PrintNow(rfh_to_use);
}

void StartBasicPrint(content::WebContents* contents) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  content::WebContents* contents_to_use = GetWebContentsToUse(contents);
  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(contents_to_use);
  if (!print_view_manager)
    return;

  content::RenderFrameHost* rfh_to_use =
      GetRenderFrameHostToUse(contents, contents_to_use);
  if (!rfh_to_use)
    return;

  print_view_manager->BasicPrint(rfh_to_use);
#endif  // ENABLE_PRINT_PREVIEW
}

content::RenderFrameHost* GetFrameToPrint(content::WebContents* contents) {
  auto* focused_frame = contents->GetFocusedFrame();
  return (focused_frame && focused_frame->HasSelection())
             ? focused_frame
             : contents->GetMainFrame();
}

content::WebContents* GetWebContentsToUse(content::WebContents* contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  guest_view::GuestViewManager* guest_view_manager =
      guest_view::GuestViewManager::FromBrowserContext(
          contents->GetBrowserContext());
  if (guest_view_manager) {
    guest_view_manager->ForEachGuest(
        contents, base::BindRepeating(&StoreFullPagePlugin, &contents));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return contents;
}

}  // namespace printing
