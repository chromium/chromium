// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/printing_api.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/quota_service.h"

namespace extensions {

PrintingSubmitJobFunction::~PrintingSubmitJobFunction() = default;

void PrintingSubmitJobFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  QuotaLimitHeuristic::Config config = {
      api::printing::MAX_SUBMIT_JOB_CALLS_PER_MINUTE, base::Minutes(1)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      config, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_SUBMIT_JOB_CALLS_PER_MINUTE"));
}

ExtensionFunction::ResponseAction PrintingSubmitJobFunction::Run() {
  std::optional<api::printing::SubmitJob::Params> params =
      api::printing::SubmitJob::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  PrintingAPIHandler::Get(browser_context())
      ->SubmitJob(ChromeExtensionFunctionDetails(this).GetNativeWindowForUI(),
                  extension_, std::move(params),
                  base::BindOnce(
                      &PrintingSubmitJobFunction::OnPrintJobSubmitted, this));

  return RespondLater();
}

void PrintingSubmitJobFunction::OnPrintJobSubmitted(
    std::optional<api::printing::SubmitJobStatus> status,
    std::optional<std::string> job_id,
    std::optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }
  api::printing::SubmitJobResponse response;
  DCHECK(status.has_value());
  response.status = status.value();
  response.job_id = std::move(job_id);
  Respond(WithArguments(response.ToValue()));
}

PrintingCancelJobFunction::~PrintingCancelJobFunction() = default;

ExtensionFunction::ResponseAction PrintingCancelJobFunction::Run() {
  std::optional<api::printing::CancelJob::Params> params =
      api::printing::CancelJob::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  std::optional<std::string> error =
      PrintingAPIHandler::Get(browser_context())
          ->CancelJob(extension_id(), params->job_id);

  if (error.has_value())
    return RespondNow(Error(error.value()));
  return RespondNow(NoArguments());
}

PrintingGetPrintersFunction::PrintingGetPrintersFunction() = default;
PrintingGetPrintersFunction::~PrintingGetPrintersFunction() = default;

ExtensionFunction::ResponseAction PrintingGetPrintersFunction::Run() {
  PrintingAPIHandler::Get(browser_context())
      ->GetPrinters(
          base::BindOnce(&PrintingGetPrintersFunction::OnPrintersReady, this));
  return RespondLater();
}

void PrintingGetPrintersFunction::OnPrintersReady(
    std::vector<api::printing::Printer> printers) {
  Respond(ArgumentList(api::printing::GetPrinters::Results::Create(printers)));
}

PrintingGetPrinterInfoFunction::~PrintingGetPrinterInfoFunction() = default;

void PrintingGetPrinterInfoFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  QuotaLimitHeuristic::Config config = {
      api::printing::MAX_GET_PRINTER_INFO_CALLS_PER_MINUTE, base::Minutes(1)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      config, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_GET_PRINTER_INFO_CALLS_PER_MINUTE"));
}

ExtensionFunction::ResponseAction PrintingGetPrinterInfoFunction::Run() {
  std::optional<api::printing::GetPrinterInfo::Params> params =
      api::printing::GetPrinterInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  PrintingAPIHandler::Get(browser_context())
      ->GetPrinterInfo(
          params->printer_id,
          base::BindOnce(
              &PrintingGetPrinterInfoFunction::OnPrinterInfoRetrieved, this));

  return RespondLater();
}

void PrintingGetPrinterInfoFunction::OnPrinterInfoRetrieved(
    std::optional<base::Value> capabilities,
    std::optional<api::printing::PrinterStatus> status,
    std::optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }
  api::printing::GetPrinterInfoResponse response;
  if (capabilities.has_value()) {
    response.capabilities.emplace();
    base::Value capabilities_value = std::move(capabilities.value());
    CHECK(capabilities_value.is_dict());
    // It's safe just to swap values here as |capabilities_value| stores exactly
    // the same object as |response.capabilities| expects.
    std::swap(response.capabilities->additional_properties,
              capabilities_value.GetDict());
  }
  DCHECK(status.has_value());
  response.status = status.value();
  Respond(WithArguments(response.ToValue()));
}

}  // namespace extensions
