// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/printing_api_handler.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/printing/cups_wrapper.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/browser/extensions/api/printing/print_job_controller.h"
#include "chrome/browser/extensions/api/printing/print_job_submitter.h"
#include "chrome/browser/extensions/api/printing/printing_api_utils.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "printing/backend/print_backend.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/printing/print_job_utils_lacros.h"
#include "chromeos/lacros/lacros_service.h"
#endif

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
    std::unique_ptr<PrintJobController> print_job_controller,
    std::unique_ptr<chromeos::CupsWrapper> cups_wrapper,
    crosapi::mojom::LocalPrinter* local_printer) {
  auto handler = std::make_unique<PrintingAPIHandler>(
      browser_context, event_router, extension_registry,
      std::move(print_job_controller), std::move(cups_wrapper), local_printer);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  handler->local_printer_version_ = crosapi::mojom::LocalPrinter::Version_;
#endif
  return handler;
}

PrintingAPIHandler::PrintingAPIHandler(content::BrowserContext* browser_context)
    : PrintingAPIHandler(browser_context,
                         EventRouter::Get(browser_context),
                         ExtensionRegistry::Get(browser_context),
                         PrintJobController::Create(),
                         chromeos::CupsWrapper::Create()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(crosapi::CrosapiManager::IsInitialized());
  local_printer_ =
      crosapi::CrosapiManager::Get()->crosapi_ash()->local_printer_ash();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    LOG(ERROR) << "Local printer not available (PrintJobControllerImpl)";
    return;
  }
  local_printer_ = service->GetRemote<crosapi::mojom::LocalPrinter>().get();
  local_printer_version_ =
      service->GetInterfaceVersion(crosapi::mojom::LocalPrinter::Uuid_);
  if (local_printer_version_ <
      int{crosapi::mojom::LocalPrinter::MethodMinVersions::
              kAddPrintJobObserverMinVersion}) {
    LOG(WARNING) << "Ash LocalPrinter version " << local_printer_version_
                 << " does not support AddExtensionPrintJobObserver().";
    return;
  }
#endif
  local_printer_->AddPrintJobObserver(
      receiver_.BindNewPipeAndPassRemoteWithVersion(),
      crosapi::mojom::PrintJobSource::kExtension, base::DoNothing());
}

PrintingAPIHandler::PrintingAPIHandler(
    content::BrowserContext* browser_context,
    EventRouter* event_router,
    ExtensionRegistry* extension_registry,
    std::unique_ptr<PrintJobController> print_job_controller,
    std::unique_ptr<chromeos::CupsWrapper> cups_wrapper,
    crosapi::mojom::LocalPrinter* local_printer)
    : browser_context_(browser_context),
      event_router_(event_router),
      extension_registry_(extension_registry),
      print_job_controller_(std::move(print_job_controller)),
      cups_wrapper_(std::move(cups_wrapper)),
      local_printer_(local_printer) {}

PrintingAPIHandler::~PrintingAPIHandler() = default;

// static
std::string PrintingAPIHandler::CreateUniqueId(const std::string& printer_id,
                                               int job_id) {
  return base::StringPrintf("%s%d", printer_id.c_str(), job_id);
}

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
    SubmitJobCallback callback) {
  // PrintingAPIHandler must outlive PrintJobSubmitter. Even if the WeakPtr
  // expires, PrintJobSubmitter will continue to access PrintingAPIHandler
  // member variables.
  PrintJobSubmitter::Run(std::make_unique<PrintJobSubmitter>(
      native_window, browser_context_, print_job_controller_.get(),
      &pdf_flattener_, std::move(extension), std::move(params->request),
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      local_printer_version_,
#endif
      local_printer_,
      base::BindOnce(&PrintingAPIHandler::OnPrintJobSubmitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void PrintingAPIHandler::OnPrintJobSubmitted(
    SubmitJobCallback callback,
    absl::optional<int> job_id,
    printing::PrintJob* print_job,
    printing::PrintedDocument* document,
    absl::optional<std::string> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!(job_id && error));

  if (!job_id) {
    absl::optional<api::printing::SubmitJobStatus> status;
    if (!error)
      status = api::printing::SUBMIT_JOB_STATUS_USER_REJECTED;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status, absl::nullopt,
                                  std::move(error)));
    return;
  }

  DCHECK_EQ(print_job->source(), crosapi::mojom::PrintJob::Source::kExtension);

  std::string printer_id =
      base::UTF16ToUTF8(document->settings().device_name());
  DCHECK(!printer_id.empty());

  std::string cups_id = CreateUniqueId(printer_id, *job_id);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), api::printing::SUBMIT_JOB_STATUS_OK,
                     cups_id, absl::nullopt));

  DCHECK(!base::Contains(print_jobs_, cups_id));
  print_jobs_[cups_id] =
      PrintJobInfo{printer_id, *job_id, print_job->source_id()};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  NotifyAshJobCreated(*print_job, *job_id, *document, local_printer_);
#endif

  if (!extension_registry_->enabled_extensions().Contains(
          print_job->source_id())) {
    return;
  }

  auto event =
      std::make_unique<Event>(events::PRINTING_ON_JOB_STATUS_CHANGED,
                              api::printing::OnJobStatusChanged::kEventName,
                              api::printing::OnJobStatusChanged::Create(
                                  cups_id, api::printing::JOB_STATUS_PENDING));
  event_router_->DispatchEventToExtension(print_job->source_id(),
                                          std::move(event));
}

absl::optional<std::string> PrintingAPIHandler::CancelJob(
    const std::string& extension_id,
    const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = print_jobs_.find(job_id);

  // If there was no print job with specified id sent by the extension return
  // an error.
  if (it == print_jobs_.end() || it->second.extension_id != extension_id) {
    return kNoActivePrintJobWithIdError;
  }

  if (!local_printer_)
    return "Local printer not available";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (local_printer_version_ <
      int{crosapi::mojom::LocalPrinter::MethodMinVersions::
              kCancelPrintJobMinVersion}) {
    LOG(WARNING) << "Ash LocalPrinter version " << local_printer_version_
                 << " does not support CancelPrintJob().";
    return "Ash local printer interface does not support CancelPrintJob()";
  }
#endif

  local_printer_->CancelPrintJob(it->second.printer_id, it->second.job_id,
                                 base::DoNothing());
  return absl::nullopt;
}

void PrintingAPIHandler::GetPrinters(GetPrintersCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!local_printer_) {
    LOG(ERROR) << "Local printer interface not available "
                  "(PrintingAPIHandler::GetPrinters()";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<api::printing::Printer>{}));
    return;
  }

  local_printer_->GetPrinters(
      base::BindOnce(&PrintingAPIHandler::OnPrintersRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintingAPIHandler::OnPrintersRetrieved(
    GetPrintersCallback callback,
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrefService* prefs =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();

  absl::optional<DefaultPrinterRules> default_printer_rules =
      GetDefaultPrinterRules(prefs->GetString(
          prefs::kPrintPreviewDefaultDestinationSelectionRules));

  auto* sticky_settings = printing::PrintPreviewStickySettings::GetInstance();
  sticky_settings->RestoreFromPrefs(prefs);
  base::flat_map<std::string, int> recently_used_ranks =
      sticky_settings->GetPrinterRecentlyUsedRanks();

  std::vector<api::printing::Printer> printers;
  printers.reserve(data.size());
  for (const crosapi::mojom::LocalDestinationInfoPtr& ptr : data) {
    printers.push_back(
        PrinterToIdl(*ptr, default_printer_rules, recently_used_ranks));
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(printers)));
}

void PrintingAPIHandler::GetPrinterInfo(const std::string& printer_id,
                                        GetPrinterInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!local_printer_) {
    LOG(ERROR)
        << "Local printer not available (PrintingAPIHandler::GetPrinterInfo)";
    OnPrinterCapabilitiesRetrieved(std::string(), std::move(callback), nullptr);
    return;
  }
  local_printer_->GetCapability(
      printer_id,
      base::BindOnce(&PrintingAPIHandler::OnPrinterCapabilitiesRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), printer_id,
                     std::move(callback)));
}

void PrintingAPIHandler::OnPrinterCapabilitiesRetrieved(
    const std::string& printer_id,
    GetPrinterInfoCallback callback,
    crosapi::mojom::CapabilitiesResponsePtr caps) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!caps) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*capabilities=*/absl::nullopt,
                       /*status=*/absl::nullopt, kInvalidPrinterIdError));
    return;
  }
  if (!caps->capabilities) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*capabilities=*/absl::nullopt,
                       /*status=*/api::printing::PRINTER_STATUS_UNREACHABLE,
                       /*error=*/absl::nullopt));
    return;
  }
  cups_wrapper_->QueryCupsPrinterStatus(
      printer_id,
      base::BindOnce(&PrintingAPIHandler::OnPrinterStatusRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     cloud_print::PrinterSemanticCapsAndDefaultsToCdd(
                         *caps->capabilities)));
}

void PrintingAPIHandler::OnPrinterStatusRetrieved(
    GetPrinterInfoCallback callback,
    base::Value capabilities,
    std::unique_ptr<::printing::PrinterStatus> printer_status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!printer_status) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(capabilities),
                                  api::printing::PRINTER_STATUS_UNREACHABLE,
                                  /*error=*/absl::nullopt));
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), std::move(capabilities),
          PrinterStatusToIdl(chromeos::PrinterErrorCodeFromPrinterStatusReasons(
              *printer_status)),
          /*error=*/absl::nullopt));
}

void PrintingAPIHandler::SetPrintJobControllerForTesting(
    std::unique_ptr<PrintJobController> print_job_controller) {
  print_job_controller_ = std::move(print_job_controller);
}

void PrintingAPIHandler::OnPrintJobUpdate(
    const std::string& printer_id,
    unsigned int job_id,
    crosapi::mojom::PrintJobStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool done = true;
  api::printing::JobStatus job_status;
  switch (status) {
    case crosapi::mojom::PrintJobStatus::kStarted:
      job_status = api::printing::JOB_STATUS_IN_PROGRESS;
      done = false;
      break;
    case crosapi::mojom::PrintJobStatus::kDone:
      job_status = api::printing::JOB_STATUS_PRINTED;
      break;
    case crosapi::mojom::PrintJobStatus::kError:
      job_status = api::printing::JOB_STATUS_FAILED;
      break;
    case crosapi::mojom::PrintJobStatus::kCancelled:
      job_status = api::printing::JOB_STATUS_CANCELED;
      break;
    default:  // crosapi::mojom::PrintJobStatus::kCreated
      return;
  }

  std::string cups_id = CreateUniqueId(printer_id, job_id);
  auto it = print_jobs_.find(cups_id);
  if (it == print_jobs_.end())
    return;
  const std::string& extension_id = it->second.extension_id;

  if (extension_registry_->enabled_extensions().Contains(extension_id)) {
    auto event = std::make_unique<Event>(
        events::PRINTING_ON_JOB_STATUS_CHANGED,
        api::printing::OnJobStatusChanged::kEventName,
        api::printing::OnJobStatusChanged::Create(cups_id, job_status));
    event_router_->DispatchEventToExtension(extension_id, std::move(event));
  }

  if (done)
    print_jobs_.erase(it);
}

template <>
KeyedService*
BrowserContextKeyedAPIFactory<PrintingAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(context);
  // We do not want an instance of PrintingAPIHandler on the lock screen.
  // This will lead to multiple printing notifications.
  if (!profile->IsRegularProfile()) {
    return nullptr;
  }
  return new PrintingAPIHandler(context);
}

}  // namespace extensions
