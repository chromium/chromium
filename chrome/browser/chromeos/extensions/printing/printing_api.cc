// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "extensions/browser/quota_service.h"

namespace extensions {

PrintingSubmitJobFunction::~PrintingSubmitJobFunction() = default;

void PrintingSubmitJobFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  QuotaLimitHeuristic::Config config = {
      api::printing::MAX_SUBMIT_JOB_CALLS_PER_MINUTE,
      base::TimeDelta::FromMinutes(1)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      config, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_SUBMIT_JOB_CALLS_PER_MINUTE"));
}

ExtensionFunction::ResponseAction PrintingSubmitJobFunction::Run() {
  std::unique_ptr<api::printing::SubmitJob::Params> params(
      api::printing::SubmitJob::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  PrintingAPIHandler::Get(browser_context())
      ->SubmitJob(ChromeExtensionFunctionDetails(this).GetNativeWindowForUI(),
                  extension_, std::move(params),
                  base::BindOnce(
                      &PrintingSubmitJobFunction::OnPrintJobSubmitted, this));

  return RespondLater();
}

void PrintingSubmitJobFunction::OnPrintJobSubmitted(
    base::Optional<api::printing::SubmitJobStatus> status,
    std::unique_ptr<std::string> job_id,
    base::Optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }
  api::printing::SubmitJobResponse response;
  DCHECK(status.has_value());
  response.status = status.value();
  response.job_id = std::move(job_id);
  Respond(OneArgument(base::Value::FromUniquePtrValue(response.ToValue())));
}

PrintingCancelJobFunction::~PrintingCancelJobFunction() = default;

ExtensionFunction::ResponseAction PrintingCancelJobFunction::Run() {
  std::unique_ptr<api::printing::CancelJob::Params> params(
      api::printing::CancelJob::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::Optional<std::string> error =
      PrintingAPIHandler::Get(browser_context())
          ->CancelJob(extension_id(), params->job_id);

  if (error.has_value())
    return RespondNow(Error(error.value()));
  return RespondNow(NoArguments());
}

PrintingGetPrintersFunction::~PrintingGetPrintersFunction() = default;

ExtensionFunction::ResponseAction PrintingGetPrintersFunction::Run() {
  return RespondNow(ArgumentList(api::printing::GetPrinters::Results::Create(
      PrintingAPIHandler::Get(browser_context())->GetPrinters())));
}

PrintingGetPrinterInfoFunction::~PrintingGetPrinterInfoFunction() = default;

void PrintingGetPrinterInfoFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  QuotaLimitHeuristic::Config config = {
      api::printing::MAX_GET_PRINTER_INFO_CALLS_PER_MINUTE,
      base::TimeDelta::FromMinutes(1)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      config, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_GET_PRINTER_INFO_CALLS_PER_MINUTE"));
}

ExtensionFunction::ResponseAction PrintingGetPrinterInfoFunction::Run() {
  std::unique_ptr<api::printing::GetPrinterInfo::Params> params(
      api::printing::GetPrinterInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  PrintingAPIHandler::Get(browser_context())
      ->GetPrinterInfo(
          params->printer_id,
          base::BindOnce(
              &PrintingGetPrinterInfoFunction::OnPrinterInfoRetrieved, this));

  return RespondLater();
}

void PrintingGetPrinterInfoFunction::OnPrinterInfoRetrieved(
    base::Optional<base::Value> capabilities,
    base::Optional<api::printing::PrinterStatus> status,
    base::Optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }
  api::printing::GetPrinterInfoResponse response;
  if (capabilities.has_value()) {
    response.capabilities =
        std::make_unique<api::printing::GetPrinterInfoResponse::Capabilities>();
    base::Value capabilities_value = std::move(capabilities.value());
    CHECK(capabilities_value.is_dict());
    // It's safe just to swap values here as |capabilities_value| stores exactly
    // the same object as |response.capabilities| expects.
    response.capabilities->additional_properties.Swap(
        static_cast<base::DictionaryValue*>(&capabilities_value));
  }
  DCHECK(status.has_value());
  response.status = status.value();
  Respond(OneArgument(base::Value::FromUniquePtrValue(response.ToValue())));
}

}  // namespace extensions
