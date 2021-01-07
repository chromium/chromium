// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printing_message_filter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/module_database.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

class PrintingMessageFilterShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static PrintingMessageFilterShutdownNotifierFactory* GetInstance() {
    return base::Singleton<PrintingMessageFilterShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<
      PrintingMessageFilterShutdownNotifierFactory>;

  PrintingMessageFilterShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "PrintingMessageFilter") {}

  ~PrintingMessageFilterShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(PrintingMessageFilterShutdownNotifierFactory);
};

}  // namespace

PrintingMessageFilter::PrintingMessageFilter(int render_process_id,
                                             Profile* profile)
    : BrowserMessageFilter(PrintMsgStart),
      render_process_id_(render_process_id),
      queue_(g_browser_process->print_job_manager()->queue()) {
  DCHECK(queue_.get());
  printing_shutdown_subscription_ =
      PrintingMessageFilterShutdownNotifierFactory::GetInstance()
          ->Get(profile)
          ->Subscribe(base::Bind(&PrintingMessageFilter::ShutdownOnUIThread,
                                 base::Unretained(this)));
  is_printing_enabled_.Init(prefs::kPrintingEnabled, profile->GetPrefs());
  is_printing_enabled_.MoveToSequence(content::GetIOThreadTaskRunner({}));
}

PrintingMessageFilter::~PrintingMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PrintingMessageFilter::ShutdownOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  is_printing_enabled_.Destroy();
  printing_shutdown_subscription_ = {};
}

void PrintingMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

bool PrintingMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintingMessageFilter, message)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    IPC_MESSAGE_HANDLER(PrintHostMsg_CheckForCancel, OnCheckForCancel)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintingMessageFilter::OnCheckForCancel(const mojom::PreviewIds& ids,
                                             bool* cancel) {
  *cancel = PrintPreviewUI::ShouldCancelRequest(ids);
}
#endif

}  // namespace printing
