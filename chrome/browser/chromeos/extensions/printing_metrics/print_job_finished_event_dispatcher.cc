// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_finished_event_dispatcher.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_info_idl_conversions.h"
#include "chrome/common/extensions/api/printing_metrics.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace extensions {

BrowserContextKeyedAPIFactory<PrintJobFinishedEventDispatcher>*
PrintJobFinishedEventDispatcher::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<PrintJobFinishedEventDispatcher>>
      instance;
  return instance.get();
}

PrintJobFinishedEventDispatcher::PrintJobFinishedEventDispatcher(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      event_router_(EventRouter::Get(browser_context)),
      print_job_history_service_observer_(this) {
  auto* history_service =
      chromeos::PrintJobHistoryServiceFactory::GetForBrowserContext(
          browser_context);

  // The print job history service is not available on the lock screen.
  if (history_service) {
    print_job_history_service_observer_.Add(history_service);
  }
}

PrintJobFinishedEventDispatcher::~PrintJobFinishedEventDispatcher() {}

void PrintJobFinishedEventDispatcher::OnPrintJobFinished(
    const chromeos::printing::proto::PrintJobInfo& print_job_info) {
  auto event = std::make_unique<Event>(
      events::PRINTING_METRICS_ON_PRINT_JOB_FINISHED,
      api::printing_metrics::OnPrintJobFinished::kEventName,
      api::printing_metrics::OnPrintJobFinished::Create(
          PrintJobInfoProtoToIdl(print_job_info)));

  event_router_->BroadcastEvent(std::move(event));
}

}  // namespace extensions
