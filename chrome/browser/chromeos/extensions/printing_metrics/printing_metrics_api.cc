// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_api.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_info_idl_conversions.h"
#include "chrome/common/extensions/api/printing_metrics.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

PrintingMetricsGetPrintJobsFunction::~PrintingMetricsGetPrintJobsFunction() =
    default;

ExtensionFunction::ResponseAction PrintingMetricsGetPrintJobsFunction::Run() {
  ash::PrintJobHistoryServiceFactory::GetForBrowserContext(browser_context())
      ->GetPrintJobs(base::BindOnce(
          &PrintingMetricsGetPrintJobsFunction::OnPrintJobsRetrieved, this));
  return RespondLater();
}

void PrintingMetricsGetPrintJobsFunction::OnPrintJobsRetrieved(
    bool success,
    std::vector<ash::printing::proto::PrintJobInfo> proto_infos) {
  std::vector<api::printing_metrics::PrintJobInfo> api_infos;
  if (success) {
    for (const auto& proto_info : proto_infos) {
      api_infos.push_back(extensions::PrintJobInfoProtoToIdl(proto_info));
    }
  }
  Respond(ArgumentList(
      api::printing_metrics::GetPrintJobs::Results::Create(api_infos)));
}

}  // namespace extensions
