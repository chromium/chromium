// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINTING_METRICS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINTING_METRICS_SERVICE_H_

#include "chromeos/crosapi/mojom/printing_metrics.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class Value;
}  // namespace base

namespace extensions {

// This class implements the chrome.printingMetrics API.
class PrintingMetricsService
    : public BrowserContextKeyedAPI,
      public EventRouter::Observer,
      public crosapi::mojom::PrintJobObserverForProfile {
 public:
  static BrowserContextKeyedAPIFactory<PrintingMetricsService>*
  GetFactoryInstance();

  static PrintingMetricsService* Get(content::BrowserContext*);

 public:
  explicit PrintingMetricsService(content::BrowserContext*);
  ~PrintingMetricsService() override;

  PrintingMetricsService(const PrintingMetricsService&) = delete;
  PrintingMetricsService& operator=(const PrintingMetricsService&) = delete;

  // Proxies chrome.printingMetrics.getPrintJobs call to the remote.
  void GetPrintJobs(
      crosapi::mojom::PrintingMetricsForProfile::GetPrintJobsCallback);

  // crosapi::mojom::PrintJobObserverForProfile:
  void OnPrintJobFinished(base::Value print_job) override;

 private:
  friend class BrowserContextKeyedAPIFactory<PrintingMetricsService>;

  // Services on the ash side are created in a lazy manner (i.e. when
  // the user tries to access the API by calling a function or registering an
  // observer). This function initializes the crosapi connection if it hasn't
  // been settled yet.
  void EnsureInit();

  // BrowserContextKeyedAPI:
  static const bool kServiceIsNULLWhileTesting = true;
  static const char* service_name() { return "PrintingMetricsService"; }
  void Shutdown() override;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo&) override;

  raw_ptr<content::BrowserContext> context_;

  bool initialized_ = false;

  mojo::Remote<crosapi::mojom::PrintingMetricsForProfile> remote_;
  mojo::Receiver<crosapi::mojom::PrintJobObserverForProfile> receiver_{this};
};

template <>
struct BrowserContextFactoryDependencies<PrintingMetricsService> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<PrintingMetricsService>* factory) {
    factory->DependsOn(EventRouterFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_METRICS_PRINTING_METRICS_SERVICE_H_
