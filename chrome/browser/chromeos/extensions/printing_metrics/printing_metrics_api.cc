// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_api.h"

#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_info_idl_conversions.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/common/extensions/api/printing_metrics.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

PrintingMetricsGetPrintJobsFunction::~PrintingMetricsGetPrintJobsFunction() =
    default;

ExtensionFunction::ResponseAction PrintingMetricsGetPrintJobsFunction::Run() {
  chromeos::PrintJobHistoryService* print_job_history_service =
      chromeos::PrintJobHistoryServiceFactory::GetForBrowserContext(
          browser_context());
  print_job_history_service->GetPrintJobs(base::BindOnce(
      &PrintingMetricsGetPrintJobsFunction::OnPrintJobsRetrieved, this));

  // GetPrintJobs might have already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PrintingMetricsGetPrintJobsFunction::OnPrintJobsRetrieved(
    bool success,
    std::unique_ptr<std::vector<chromeos::printing::proto::PrintJobInfo>>
        print_job_info_protos) {
  std::vector<api::printing_metrics::PrintJobInfo> print_job_infos;
  if (success && print_job_info_protos) {
    for (const auto& print_job_info_proto : *print_job_info_protos)
      print_job_infos.push_back(PrintJobInfoProtoToIdl(print_job_info_proto));
  }
  Respond(ArgumentList(
      api::printing_metrics::GetPrintJobs::Results::Create(print_job_infos)));
}

}  // namespace extensions
