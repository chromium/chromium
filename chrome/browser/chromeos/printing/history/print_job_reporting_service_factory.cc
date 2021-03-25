// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_reporting_service_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/util/backoff_settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

// static
PrintJobReportingService* PrintJobReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrintJobReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrintJobReportingServiceFactory*
PrintJobReportingServiceFactory::GetInstance() {
  return base::Singleton<PrintJobReportingServiceFactory>::get();
}

PrintJobReportingServiceFactory::PrintJobReportingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PrintJobReportingServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

PrintJobReportingServiceFactory::~PrintJobReportingServiceFactory() = default;

KeyedService* PrintJobReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  auto reporting_service = PrintJobReportingService::Create();
  ReportQueueFactory::Create(profile,
                             reporting_service->GetReportQueueSetter());

  return reporting_service.release();
}

bool PrintJobReportingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

// static
void ReportQueueFactory::Create(Profile* profile, SuccessCallback success_cb) {
  policy::DMToken dm_token =
      policy::GetDMToken(profile, /*only_affiliated=*/false);
  if (!dm_token.is_valid()) {
    VLOG(1) << "DMToken must be valid";
    return;
  }

  auto config_result = reporting::ReportQueueConfiguration::Create(
      dm_token.value(), reporting::Destination::PRINT_JOBS,
      base::BindRepeating([]() { return reporting::Status::StatusOK(); }));
  if (!config_result.ok()) {
    VLOG(1) << "ReportQueueConfiguration must be valid";
    return;
  }

  // Asynchronously create and try to set ReportQueue.
  auto try_set_cb = CreateTrySetCallback(dm_token, std::move(success_cb),
                                         reporting::GetBackoffEntry());
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(reporting::ReportQueueProvider::CreateQueue,
                                std::move(config_result.ValueOrDie()),
                                std::move(try_set_cb)));
}

void ReportQueueFactory::Retry(policy::DMToken dm_token,
                               std::unique_ptr<net::BackoffEntry> backoff_entry,
                               SuccessCallback success_cb) {
  auto config_result = reporting::ReportQueueConfiguration::Create(
      dm_token.value(), reporting::Destination::PRINT_JOBS,
      base::BindRepeating([]() { return reporting::Status::StatusOK(); }));
  if (!config_result.ok()) {
    VLOG(1) << "ReportQueueConfiguration must be valid";
    return;
  }

  // Asynchronously create and try to set ReportQueue.
  backoff_entry->InformOfRequest(/*succeeded=*/false);
  base::TimeDelta time_until_release = backoff_entry->GetTimeUntilRelease();
  auto try_set_cb = CreateTrySetCallback(dm_token, std::move(success_cb),
                                         std::move(backoff_entry));
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      base::BindOnce(reporting::ReportQueueProvider::CreateQueue,
                     std::move(config_result.ValueOrDie()),
                     std::move(try_set_cb)),
      time_until_release);
}

reporting::ReportQueueProvider::CreateReportQueueCallback
ReportQueueFactory::CreateTrySetCallback(
    policy::DMToken dm_token,
    SuccessCallback success_cb,
    std::unique_ptr<net::BackoffEntry> backoff_entry) {
  auto retry_cb = base::BindOnce(&ReportQueueFactory::Retry, dm_token,
                                 std::move(backoff_entry));
  return base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&ReportQueueFactory::TrySetReportQueue,
                     std::move(success_cb), std::move(retry_cb)));
}

// static
void ReportQueueFactory::TrySetReportQueue(
    SuccessCallback success_cb,
    base::OnceCallback<void(SuccessCallback)> retry_cb,
    reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>
        report_queue_result) {
  if (!report_queue_result.ok()) {
    VLOG(1) << "ReportQueue could not be created";
    return;
  }
  std::move(success_cb).Run(std::move(report_queue_result.ValueOrDie()));
}

}  // namespace chromeos
