// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printing_init.h"

#include "chrome/browser/headless/headless_mode_util.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/printing/browser/headless/headless_print_manager.h"
#include "components/printing/browser/print_manager_utils.h"
#include "content/public/browser/web_contents.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "base/functional/bind.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "content/public/browser/browser_thread.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/pdf_nup_converter_client.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

namespace printing {

#if BUILDFLAG(ENABLE_OOP_PRINTING)
void EarlyStartPrintBackendService() {
  AfterStartupTaskUtils::PostTask(
      FROM_HERE, content::GetUIThreadTaskRunner({}),
      base::BindOnce(&PrintBackendServiceManager::LaunchPersistentService));
}
#endif

void InitializePrintingForWebContents(content::WebContents* web_contents) {
  // Headless mode uses a minimalistic Print Manager implementation that
  // shortcuts most of the callbacks providing only print to PDF
  // functionality.
  if (headless::IsHeadlessMode()) {
    headless::HeadlessPrintManager::CreateForWebContents(web_contents);
  } else {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    PrintViewManager::CreateForWebContents(web_contents);
    PdfNupConverterClient::CreateForWebContents(web_contents);
#else
    PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  }
  CreateCompositeClientIfNeeded(web_contents, embedder_support::GetUserAgent());
}

}  // namespace printing
