// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api_utils.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_wrapper.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "printing/backend/cups_jobs.h"
#include "printing/backend/print_backend.h"

namespace extensions {

namespace {

constexpr char kInvalidPrinterIdError[] = "Invalid printer ID";
constexpr char kNoActivePrintJobWithIdError[] =
    "No active print job with given ID";

}  // namespace

// static
std::unique_ptr<PrintingAPIHandler> PrintingAPIHandler::CreateForTesting(
    content::BrowserContext* browser_context,
    EventRouter* event_router,
    ExtensionRegistry* extension_registry,
    chromeos::CupsPrintJobManager* print_job_manager,
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<PrintJobController> print_job_controller,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
    std::unique_ptr<chromeos::CupsWrapper> cups_wrapper) {
  return base::WrapUnique(new PrintingAPIHandler(
      browser_context, event_router, extension_registry, print_job_manager,
      printers_manager, std::move(print_job_controller),
      std::move(printer_configurer), std::move(cups_wrapper)));
}

PrintingAPIHandler::PrintingAPIHandler(content::BrowserContext* browser_context)
    : PrintingAPIHandler(
          browser_context,
          EventRouter::Get(browser_context),
          ExtensionRegistry::Get(browser_context),
          chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(
              browser_context),
          chromeos::CupsPrintersManagerFactory::GetForBrowserContext(
              browser_context),
          PrintJobController::Create(
              chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(
                  browser_context)),
          chromeos::PrinterConfigurer::Create(
              Profile::FromBrowserContext(browser_context)),
          chromeos::CupsWrapper::Create()) {}

PrintingAPIHandler::PrintingAPIHandler(
    content::BrowserContext* browser_context,
    EventRouter* event_router,
    ExtensionRegistry* extension_registry,
    chromeos::CupsPrintJobManager* print_job_manager,
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<PrintJobController> print_job_controller,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
    std::unique_ptr<chromeos::CupsWrapper> cups_wrapper)
    : browser_context_(browser_context),
      event_router_(event_router),
      extension_registry_(extension_registry),
      print_job_manager_(print_job_manager),
      printers_manager_(printers_manager),
      print_job_controller_(std::move(print_job_controller)),
      printer_capabilities_provider_(printers_manager,
                                     std::move(printer_configurer)),
      cups_wrapper_(std::move(cups_wrapper)),
      print_job_manager_observer_(this) {
  print_job_manager_observer_.Add(print_job_manager_);
}

PrintingAPIHandler::~PrintingAPIHandler() = default;

// static
BrowserContextKeyedAPIFactory<PrintingAPIHandler>*
PrintingAPIHandler::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<PrintingAPIHandler>>
      instance;
  return instance.get();
}

// static
PrintingAPIHandler* PrintingAPIHandler::Get(
    content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<PrintingAPIHandler>::Get(
      browser_context);
}

// static
void PrintingAPIHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kPrintingAPIExtensionsAllowlist);
}

void PrintingAPIHandler::SubmitJob(
    gfx::NativeWindow native_window,
    scoped_refptr<const extensions::Extension> extension,
    std::unique_ptr<api::printing::SubmitJob::Params> params,
    PrintJobSubmitter::SubmitJobCallback callback) {
  auto print_job_submitter = std::make_unique<PrintJobSubmitter>(
      native_window, browser_context_, printers_manager_,
      &printer_capabilities_provider_, print_job_controller_.get(),
      &pdf_flattener_, std::move(extension), std::move(params->request));
  PrintJobSubmitter* print_job_submitter_ptr = print_job_submitter.get();
  print_job_submitter_ptr->Start(base::BindOnce(
      &PrintingAPIHandler::OnPrintJobSubmitted, weak_ptr_factory_.GetWeakPtr(),
      std::move(print_job_submitter), std::move(callback)));
}

void PrintingAPIHandler::OnPrintJobSubmitted(
    std::unique_ptr<PrintJobSubmitter> print_job_submitter,
    PrintJobSubmitter::SubmitJobCallback callback,
    base::Optional<api::printing::SubmitJobStatus> status,
    std::unique_ptr<std::string> job_id,
    base::Optional<std::string> error) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), status, std::move(job_id), error));
}

base::Optional<std::string> PrintingAPIHandler::CancelJob(
    const std::string& extension_id,
    const std::string& job_id) {
  auto it = print_jobs_extension_ids_.find(job_id);
  // If there was no print job with specified id sent by the extension return
  // an error.
  if (it == print_jobs_extension_ids_.end() || it->second != extension_id)
    return kNoActivePrintJobWithIdError;

  // If we can't cancel the print job (e.g. it's in terminated state) return an
  // error.
  if (!print_job_controller_->CancelPrintJob(job_id))
    return kNoActivePrintJobWithIdError;

  // Return no error otherwise.
  return base::nullopt;
}

std::vector<api::printing::Printer> PrintingAPIHandler::GetPrinters() {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();

  base::Optional<DefaultPrinterRules> default_printer_rules =
      GetDefaultPrinterRules(prefs->GetString(
          prefs::kPrintPreviewDefaultDestinationSelectionRules));

  auto* sticky_settings = printing::PrintPreviewStickySettings::GetInstance();
  sticky_settings->RestoreFromPrefs(prefs);
  base::flat_map<std::string, int> recently_used_ranks =
      sticky_settings->GetPrinterRecentlyUsedRanks();

  static constexpr chromeos::PrinterClass kPrinterClasses[] = {
      chromeos::PrinterClass::kEnterprise,
      chromeos::PrinterClass::kSaved,
      chromeos::PrinterClass::kAutomatic,
  };
  std::vector<api::printing::Printer> all_printers;
  for (auto printer_class : kPrinterClasses) {
    const std::vector<chromeos::Printer>& printers =
        printers_manager_->GetPrinters(printer_class);
    for (const auto& printer : printers) {
      all_printers.emplace_back(
          PrinterToIdl(printer, default_printer_rules, recently_used_ranks));
    }
  }
  return all_printers;
}

void PrintingAPIHandler::GetPrinterInfo(const std::string& printer_id,
                                        GetPrinterInfoCallback callback) {
  if (!printers_manager_->GetPrinter(printer_id)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*capabilities=*/base::nullopt,
                       /*status=*/base::nullopt, kInvalidPrinterIdError));
    return;
  }
  printer_capabilities_provider_.GetPrinterCapabilities(
      printer_id, base::BindOnce(&PrintingAPIHandler::GetPrinterStatus,
                                 weak_ptr_factory_.GetWeakPtr(), printer_id,
                                 std::move(callback)));
}

void PrintingAPIHandler::GetPrinterStatus(
    const std::string& printer_id,
    GetPrinterInfoCallback callback,
    base::Optional<printing::PrinterSemanticCapsAndDefaults> capabilities) {
  if (!capabilities.has_value()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*capabilities=*/base::nullopt,
                       api::printing::PRINTER_STATUS_UNREACHABLE,
                       /*error=*/base::nullopt));
    return;
  }
  base::Value capabilities_value =
      cloud_print::PrinterSemanticCapsAndDefaultsToCdd(capabilities.value());
  cups_wrapper_->QueryCupsPrinterStatus(
      printer_id,
      base::BindOnce(&PrintingAPIHandler::OnPrinterStatusRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(capabilities_value)));
}

void PrintingAPIHandler::OnPrinterStatusRetrieved(
    GetPrinterInfoCallback callback,
    base::Value capabilities,
    std::unique_ptr<::printing::PrinterStatus> printer_status) {
  if (!printer_status) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(capabilities),
                                  api::printing::PRINTER_STATUS_UNREACHABLE,
                                  /*error=*/base::nullopt));
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), std::move(capabilities),
          PrinterStatusToIdl(chromeos::PrinterErrorCodeFromPrinterStatusReasons(
              *printer_status)),
          /*error=*/base::nullopt));
}

void PrintingAPIHandler::SetPrintJobControllerForTesting(
    std::unique_ptr<PrintJobController> print_job_controller) {
  print_job_controller_ = std::move(print_job_controller);
}

void PrintingAPIHandler::OnPrintJobCreated(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_PENDING, job);
  if (!job || job->source() != printing::PrintJob::Source::EXTENSION)
    return;
  const std::string& extension_id = job->source_id();
  const std::string& job_id = job->GetUniqueId();
  print_jobs_extension_ids_[job_id] = extension_id;
  print_job_controller_->OnPrintJobCreated(extension_id, job_id, job);
}

void PrintingAPIHandler::OnPrintJobStarted(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_IN_PROGRESS, job);
}

void PrintingAPIHandler::OnPrintJobDone(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_PRINTED, job);
  FinishJob(job);
}

void PrintingAPIHandler::OnPrintJobError(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_FAILED, job);
  FinishJob(job);
}

void PrintingAPIHandler::OnPrintJobCancelled(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_CANCELED, job);
  FinishJob(job);
}

void PrintingAPIHandler::DispatchJobStatusChangedEvent(
    api::printing::JobStatus job_status,
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  if (!job || job->source() != printing::PrintJob::Source::EXTENSION)
    return;

  auto event =
      std::make_unique<Event>(events::PRINTING_ON_JOB_STATUS_CHANGED,
                              api::printing::OnJobStatusChanged::kEventName,
                              api::printing::OnJobStatusChanged::Create(
                                  job->GetUniqueId(), job_status));

  if (extension_registry_->enabled_extensions().Contains(job->source_id()))
    event_router_->DispatchEventToExtension(job->source_id(), std::move(event));
}

void PrintingAPIHandler::FinishJob(base::WeakPtr<chromeos::CupsPrintJob> job) {
  if (!job || job->source() != printing::PrintJob::Source::EXTENSION)
    return;
  const std::string& job_id = job->GetUniqueId();
  print_jobs_extension_ids_.erase(job_id);
  print_job_controller_->OnPrintJobFinished(job_id);
}

template <>
KeyedService*
BrowserContextKeyedAPIFactory<PrintingAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // We do not want an instance of PrintingAPIHandler on the lock screen.
  // This will lead to multiple printing notifications.
  if (!chromeos::ProfileHelper::IsRegularProfile(profile)) {
    return nullptr;
  }
  return new PrintingAPIHandler(context);
}

}  // namespace extensions
