// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/printing/history/print_job_reporting_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider.h"
#include "net/base/backoff_entry.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace chromeos {

// Singleton that owns all PrintJobReportingServices and associates them with
// Profiles.
class PrintJobReportingServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PrintJobReportingService for |context|, creating it if it is
  // not yet created.
  static PrintJobReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  static PrintJobReportingServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PrintJobReportingServiceFactory>;

  PrintJobReportingServiceFactory();
  PrintJobReportingServiceFactory(const PrintJobReportingServiceFactory&) =
      delete;
  PrintJobReportingServiceFactory& operator=(
      const PrintJobReportingServiceFactory&) = delete;
  ~PrintJobReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

class ReportQueueFactory {
 public:
  using SuccessCallback =
      base::OnceCallback<void(std::unique_ptr<reporting::ReportQueue>)>;

  static void Create(Profile* profile, SuccessCallback done_cb);

 private:
  static void Retry(policy::DMToken dm_token,
                    std::unique_ptr<net::BackoffEntry> backoff_entry,
                    SuccessCallback success_cb);

  static void TrySetReportQueue(
      SuccessCallback success_cb,
      base::OnceCallback<void(SuccessCallback)> failure_cb,
      reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>
          report_queue_result);

  static reporting::ReportQueueProvider::CreateReportQueueCallback
  CreateTrySetCallback(policy::DMToken dm_token,
                       SuccessCallback success_cb,
                       std::unique_ptr<net::BackoffEntry> backoff_entry);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_FACTORY_H_
