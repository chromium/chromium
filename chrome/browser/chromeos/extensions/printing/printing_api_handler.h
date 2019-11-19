// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/common/extensions/api/printing.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class ExtensionRegistry;

// Observes CupsPrintJobManager and generates OnJobStatusChanged() events of
// chrome.printing API.
class PrintingAPIHandler : public BrowserContextKeyedAPI,
                           public chromeos::CupsPrintJobManager::Observer {
 public:
  explicit PrintingAPIHandler(content::BrowserContext* browser_context);
  ~PrintingAPIHandler() override;

  // BrowserContextKeyedAPI:
  static BrowserContextKeyedAPIFactory<PrintingAPIHandler>*
  GetFactoryInstance();

  // Returns the current instance for |browser_context|.
  static PrintingAPIHandler* Get(content::BrowserContext* browser_context);

 private:
  // Needed for BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<PrintingAPIHandler>;

  // CupsPrintJobManager::Observer:
  void OnPrintJobCreated(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobStarted(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobDone(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<chromeos::CupsPrintJob> job) override;

  void DispatchJobStatusChangedEvent(api::printing::JobStatus job_status,
                                     base::WeakPtr<chromeos::CupsPrintJob> job);

  // BrowserContextKeyedAPI:
  static const bool kServiceIsNULLWhileTesting = true;
  static const char* service_name() { return "PrintingAPIHandler"; }

  content::BrowserContext* const browser_context_;
  EventRouter* const event_router_;
  ExtensionRegistry* const extension_registry_;

  chromeos::CupsPrintJobManager* print_job_manager_;
  ScopedObserver<chromeos::CupsPrintJobManager,
                 chromeos::CupsPrintJobManager::Observer>
      print_job_manager_observer_;

  base::WeakPtrFactory<PrintingAPIHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintingAPIHandler);
};

template <>
struct BrowserContextFactoryDependencies<PrintingAPIHandler> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<PrintingAPIHandler>* factory) {
    factory->DependsOn(EventRouterFactory::GetInstance());
    factory->DependsOn(chromeos::CupsPrintJobManagerFactory::GetInstance());
  }
};

template <>
KeyedService*
BrowserContextKeyedAPIFactory<PrintingAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_HANDLER_H_
