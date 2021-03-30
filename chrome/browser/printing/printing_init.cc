// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printing_init.h"

#include "components/embedder_support/user_agent_utils.h"
#include "components/printing/browser/print_manager_utils.h"
#include "content/public/browser/web_contents.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/pdf_nup_converter_client.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

namespace printing {

void InitializePrinting(content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PdfNupConverterClient::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  CreateCompositeClientIfNeeded(web_contents, embedder_support::GetUserAgent());
}

}  // namespace printing
