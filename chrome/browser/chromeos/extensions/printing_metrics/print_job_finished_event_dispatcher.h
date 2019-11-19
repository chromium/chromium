// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINT_JOB_FINISHED_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINT_JOB_FINISHED_EVENT_DISPATCHER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class EventRouter;

// PrintJobFinishedEventDispatcher observes changes in the print job history
// service and dispatches them to extensions listening on the
// printingMetrics.onPrintJobFinished() event.
class PrintJobFinishedEventDispatcher
    : public BrowserContextKeyedAPI,
      public chromeos::PrintJobHistoryService::Observer {
 public:
  explicit PrintJobFinishedEventDispatcher(
      content::BrowserContext* browser_context);
  ~PrintJobFinishedEventDispatcher() override;

  // BrowserContextKeyedAPI:
  static BrowserContextKeyedAPIFactory<PrintJobFinishedEventDispatcher>*
  GetFactoryInstance();

  // PrintJobHistoryService::Observer:
  void OnPrintJobFinished(
      const chromeos::printing::proto::PrintJobInfo& print_job_info) override;

 private:
  // Needed for BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<PrintJobFinishedEventDispatcher>;

  // BrowserContextKeyedAPI implementation.
  static const bool kServiceIsNULLWhileTesting = true;
  static const char* service_name() {
    return "PrintJobFinishedEventDispatcher";
  }

  content::BrowserContext* browser_context_;
  EventRouter* event_router_;
  ScopedObserver<chromeos::PrintJobHistoryService,
                 chromeos::PrintJobHistoryService::Observer>
      print_job_history_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(PrintJobFinishedEventDispatcher);
};

template <>
struct BrowserContextFactoryDependencies<PrintJobFinishedEventDispatcher> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<PrintJobFinishedEventDispatcher>* factory) {
    factory->DependsOn(EventRouterFactory::GetInstance());
    factory->DependsOn(chromeos::PrintJobHistoryServiceFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINT_JOB_FINISHED_EVENT_DISPATCHER_H_
