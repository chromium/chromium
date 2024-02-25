// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_api.h"

#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_service.h"
#include "chrome/common/extensions/api/printing_metrics.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

PrintingMetricsGetPrintJobsFunction::~PrintingMetricsGetPrintJobsFunction() =
    default;

ExtensionFunction::ResponseAction PrintingMetricsGetPrintJobsFunction::Run() {
  auto* service = PrintingMetricsService::Get(browser_context());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!service) {
    return RespondNow(Error("API is not accessible."));
  }
#else
  CHECK(service);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  service->GetPrintJobs(base::BindOnce(
      &PrintingMetricsGetPrintJobsFunction::OnPrintJobsRetrieved, this));

  return RespondLater();
}

void PrintingMetricsGetPrintJobsFunction::OnPrintJobsRetrieved(
    base::Value::List print_jobs) {
  std::vector<api::printing_metrics::PrintJobInfo> print_job_infos;
  for (const auto& print_job : print_jobs) {
    std::optional<api::printing_metrics::PrintJobInfo> print_job_info =
        api::printing_metrics::PrintJobInfo::FromValue(print_job);
    DCHECK(print_job_info);
    print_job_infos.emplace_back(std::move(print_job_info).value());
  }
  Respond(ArgumentList(
      api::printing_metrics::GetPrintJobs::Results::Create(print_job_infos)));
}

}  // namespace extensions
