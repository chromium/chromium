// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/print/arc_print_service.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/child_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/metafile_skia.h"
#include "printing/print_job_constants.h"
#include "printing/printed_document.h"
#include "printing/units.h"

namespace arc {
namespace {

class PrintJobHostImpl;
class PrinterDiscoverySessionHostImpl;

class ArcPrintServiceImpl : public ArcPrintService,
                            public chromeos::CupsPrintJobManager::Observer,
                            public KeyedService {
 public:
  ArcPrintServiceImpl(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);
  ~ArcPrintServiceImpl() override;

  // KeyedService:
  void Shutdown() override;

  // mojom::PrintHost:
  void PrintDeprecated(mojo::ScopedHandle pdf_data) override;
  void Print(mojom::PrintJobInstancePtr instance,
             mojom::PrintJobRequestPtr print_job,
             PrintCallback callback) override;
  void CreateDiscoverySession(
      mojom::PrinterDiscoverySessionInstancePtr instance,
      CreateDiscoverySessionCallback callback) override;

  void DeleteJob(PrintJobHostImpl* job);
  void DeleteSession(PrinterDiscoverySessionHostImpl* session);
  void JobIdGenerated(PrintJobHostImpl* job, const std::string& job_id);

 protected:
  // chromeos::CupsPrintJobManager::Observer:
  void OnPrintJobCreated(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobDone(base::WeakPtr<chromeos::CupsPrintJob> job) override;

 private:
  Profile* const profile_;                      // Owned by ProfileManager.
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  std::map<PrintJobHostImpl*, std::unique_ptr<PrintJobHostImpl>> jobs_;
  std::map<PrinterDiscoverySessionHostImpl*,
           std::unique_ptr<PrinterDiscoverySessionHostImpl>>
      sessions_;

  // Managed by PrintJobHostImpl instances.
  std::map<std::string, PrintJobHostImpl*> jobs_by_id_;
};

// Singleton factory for ArcPrintService.
class ArcPrintServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPrintServiceImpl,
          ArcPrintServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPrintServiceFactory";

  static ArcPrintServiceFactory* GetInstance() {
    return base::Singleton<ArcPrintServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPrintServiceFactory>;
  ArcPrintServiceFactory() = default;
  ~ArcPrintServiceFactory() override = default;
};

// This creates a Metafile instance which is a wrapper around a byte buffer at
// this point.
std::unique_ptr<printing::MetafileSkia> ReadFileOnBlockingTaskRunner(
    base::File file,
    size_t data_size) {
  // TODO(vkuzkokov) Can we make give pipe to CUPS directly?
  std::vector<char> buf(data_size);
  int bytes = file.ReadAtCurrentPos(buf.data(), data_size);
  if (bytes < 0) {
    PLOG(ERROR) << "Error reading PDF";
    return nullptr;
  }
  if (static_cast<size_t>(bytes) != data_size)
    return nullptr;

  file.Close();

  auto metafile = std::make_unique<printing::MetafileSkia>();
  if (!metafile->InitFromData(buf.data(), buf.size())) {
    LOG(ERROR) << "Failed to initialize PDF metafile";
    return nullptr;
  }
  return metafile;
}

using PrinterQueryCallback =
    base::OnceCallback<void(scoped_refptr<printing::PrinterQuery>)>;

void OnSetSettingsDoneOnIOThread(scoped_refptr<printing::PrinterQuery> query,
                                 PrinterQueryCallback callback);

void CreateQueryOnIOThread(std::unique_ptr<printing::PrintSettings> settings,
                           PrinterQueryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto query = base::MakeRefCounted<printing::PrinterQuery>(
      content::ChildProcessHost::kInvalidUniqueID,
      content::ChildProcessHost::kInvalidUniqueID);
  query->SetSettingsFromPOD(
      std::move(settings),
      base::BindOnce(&OnSetSettingsDoneOnIOThread, query, std::move(callback)));
}

// Send initialized PrinterQuery to UI thread.
void OnSetSettingsDoneOnIOThread(scoped_refptr<printing::PrinterQuery> query,
                                 PrinterQueryCallback callback) {
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(std::move(callback), query));
}

std::unique_ptr<printing::PrinterSemanticCapsAndDefaults>
FetchCapabilitiesOnBlockingTaskRunner(const std::string& printer_id) {
  scoped_refptr<printing::PrintBackend> backend(
      printing::PrintBackend::CreateInstance(nullptr));
  auto caps = std::make_unique<printing::PrinterSemanticCapsAndDefaults>();
  if (!backend->GetPrinterSemanticCapsAndDefaults(printer_id, caps.get())) {
    LOG(ERROR) << "Failed to get caps for " << printer_id;
    return nullptr;
  }
  return caps;
}

// Transform printer info to Mojo type and add capabilities, if present.
mojom::PrinterInfoPtr ToArcPrinter(
    const chromeos::Printer& printer,
    std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> caps) {
  return mojom::PrinterInfo::New(
      printer.id(), printer.display_name(), mojom::PrinterStatus::IDLE,
      printer.description(), base::nullopt,
      caps ? base::make_optional<printing::PrinterSemanticCapsAndDefaults>(
                 std::move(*caps))
           : base::nullopt);
}

// PrinterDiscoverySessionHost implementation.
class PrinterDiscoverySessionHostImpl
    : public mojom::PrinterDiscoverySessionHost,
      public chromeos::CupsPrintersManager::Observer {
 public:
  PrinterDiscoverySessionHostImpl(
      mojo::InterfaceRequest<mojom::PrinterDiscoverySessionHost> request,
      mojom::PrinterDiscoverySessionInstancePtr instance,
      ArcPrintServiceImpl* service,
      Profile* profile)
      : binding_(this, std::move(request)),
        instance_(std::move(instance)),
        service_(service),
        printers_manager_(
            chromeos::CupsPrintersManagerFactory::GetForBrowserContext(
                profile)),
        configurer_(chromeos::PrinterConfigurer::Create(profile)),
        weak_ptr_factory_(this) {
    printers_manager_->AddObserver(this);
    binding_.set_connection_error_handler(MakeErrorHandler());
    instance_.set_connection_error_handler(MakeErrorHandler());
  }

  ~PrinterDiscoverySessionHostImpl() override {
    printers_manager_->RemoveObserver(this);
  }

  // mojom::PrinterDiscoverySessionHost:
  void StartPrinterDiscovery(
      const std::vector<std::string>& printer_ids) override {
    std::vector<mojom::PrinterInfoPtr> arc_printers;
    for (size_t i = 0; i < chromeos::CupsPrintersManager::kNumPrinterClasses;
         i++) {
      std::vector<chromeos::Printer> printers = printers_manager_->GetPrinters(
          static_cast<chromeos::CupsPrintersManager::PrinterClass>(i));
      for (const auto& printer : printers)
        arc_printers.emplace_back(ToArcPrinter(printer, nullptr));
    }
    if (!arc_printers.empty())
      instance_->AddPrinters(std::move(arc_printers));
  }

  void StopPrinterDiscovery() override {
    // Do nothing
  }

  void ValidatePrinters(const std::vector<std::string>& printer_ids) override {
    // TODO(vkuzkokov) implement or determine that we don't need to.
  }

  void StartPrinterStateTracking(const std::string& printer_id) override {
    std::unique_ptr<chromeos::Printer> printer =
        printers_manager_->GetPrinter(printer_id);
    if (!printer) {
      RemovePrinter(printer_id);
      return;
    }
    if (printers_manager_->IsPrinterInstalled(*printer)) {
      PrinterInstalled(std::move(printer), chromeos::kSuccess);
      return;
    }
    const chromeos::Printer& printer_ref = *printer;
    configurer_->SetUpPrinter(
        printer_ref,
        base::BindOnce(&PrinterDiscoverySessionHostImpl::PrinterInstalled,
                       weak_ptr_factory_.GetWeakPtr(), std::move(printer)));
  }

  void StopPrinterStateTracking(const std::string& printer_id) override {
    // Do nothing
  }

  void DestroyDiscoverySession() override { service_->DeleteSession(this); }

  // chromeos::CupsPrintersManager::Observer:
  void OnPrintersChanged(
      chromeos::CupsPrintersManager::PrinterClass printer_class,
      const std::vector<chromeos::Printer>& printers) override {
    // TODO(vkuzkokov) remove missing printers and only add new ones.
    std::vector<mojom::PrinterInfoPtr> arc_printers;
    for (const auto& printer : printers)
      arc_printers.emplace_back(ToArcPrinter(printer, nullptr));

    instance_->AddPrinters(std::move(arc_printers));
  }

 private:
  base::OnceClosure MakeErrorHandler() {
    return base::BindOnce(
        &PrinterDiscoverySessionHostImpl::DestroyDiscoverySession,
        weak_ptr_factory_.GetWeakPtr());
  }

  // Fetch capabilities for newly installed printer.
  void PrinterInstalled(std::unique_ptr<chromeos::Printer> printer,
                        chromeos::PrinterSetupResult result) {
    if (result != chromeos::kSuccess) {
      RemovePrinter(printer->id());
      return;
    }
    printers_manager_->PrinterInstalled(*printer, true /*is_automatic*/);
    const std::string& printer_id = printer->id();
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&FetchCapabilitiesOnBlockingTaskRunner, printer_id),
        base::BindOnce(&PrinterDiscoverySessionHostImpl::CapabilitiesReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(printer)));
  }

  // Remove from the list of available printers.
  void RemovePrinter(const std::string& printer_id) {
    instance_->RemovePrinters(std::vector<std::string>{printer_id});
  }

  // Transform printer capabilities to mojo type and send to container.
  void CapabilitiesReceived(
      std::unique_ptr<chromeos::Printer> printer,
      std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> caps) {
    if (!caps) {
      RemovePrinter(printer->id());
      return;
    }
    std::vector<mojom::PrinterInfoPtr> arc_printers;
    arc_printers.emplace_back(ToArcPrinter(*printer, std::move(caps)));
    instance_->AddPrinters(std::move(arc_printers));
  }

  // Binds |this|.
  mojo::Binding<mojom::PrinterDiscoverySessionHost> binding_;

  mojom::PrinterDiscoverySessionInstancePtr instance_;
  ArcPrintServiceImpl* const service_;
  chromeos::CupsPrintersManager* printers_manager_;
  std::unique_ptr<chromeos::PrinterConfigurer> configurer_;
  base::WeakPtrFactory<PrinterDiscoverySessionHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrinterDiscoverySessionHostImpl);
};

// Get requested color mode from Mojo type.
// |mode| is a bitfield but must have exactly one mode set here.
printing::ColorModel FromArcColorMode(mojom::PrintColorMode mode) {
  switch (mode) {
    case mojom::PrintColorMode::MONOCHROME:
      return printing::GRAY;
    case mojom::PrintColorMode::COLOR:
      return printing::COLOR;
  }
  NOTREACHED();
}

// Get requested duplex mode from Mojo type.
// |mode| is a bitfield but must have exactly one mode set here.
printing::DuplexMode FromArcDuplexMode(mojom::PrintDuplexMode mode) {
  switch (mode) {
    case mojom::PrintDuplexMode::NONE:
      return printing::SIMPLEX;
    case mojom::PrintDuplexMode::LONG_EDGE:
      return printing::LONG_EDGE;
    case mojom::PrintDuplexMode::SHORT_EDGE:
      return printing::SHORT_EDGE;
  }
  NOTREACHED();
}

// This represents a single request from container. Object of this class
// self-destructs when the request is completed, successfully or otherwise.
class PrintJobHostImpl : public mojom::PrintJobHost,
                         public content::NotificationObserver {
 public:
  PrintJobHostImpl(mojo::InterfaceRequest<mojom::PrintJobHost> request,
                   mojom::PrintJobInstancePtr instance,
                   ArcPrintServiceImpl* service,
                   chromeos::CupsPrintJobManager* job_manager,
                   std::unique_ptr<printing::PrintSettings> settings,
                   base::File file,
                   size_t data_size)
      : binding_(this, std::move(request)),
        instance_(std::move(instance)),
        service_(service),
        job_manager_(job_manager),
        weak_ptr_factory_(this) {
    // We read printing data from pipe on working thread in parallel with
    // initializing PrinterQuery on IO thread. When both tasks are complete we
    // start printing.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ReadFileOnBlockingTaskRunner, std::move(file),
                       data_size),
        base::BindOnce(&PrintJobHostImpl::OnFileRead,
                       weak_ptr_factory_.GetWeakPtr()));
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&CreateQueryOnIOThread, std::move(settings),
                       base::BindOnce(&PrintJobHostImpl::OnSetSettingsDone,
                                      weak_ptr_factory_.GetWeakPtr())));
    binding_.set_connection_error_handler(MakeErrorHandler());
    instance_.set_connection_error_handler(MakeErrorHandler());
  }

  void CupsJobCreated(chromeos::CupsPrintJob* cups_job) {
    cups_job_ = cups_job;
  }

  void JobCanceled() {
    instance_->Cancel();
    service_->DeleteJob(this);
  }

  void JobError() {
    // TODO(vkuzkokov) transform cups_job_->error_code() into localized string.
    instance_->Fail({});
    service_->DeleteJob(this);
  }

  void JobDone() {
    instance_->Complete();
    service_->DeleteJob(this);
  }

  // mojom::PrintJobHost:
  void Cancel() override {
    if (cups_job_) {
      // Job already spooled.
      job_manager_->CancelPrintJob(cups_job_);
    } else {
      JobCanceled();
    }
  }

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(chrome::NOTIFICATION_PRINT_JOB_EVENT, type);
    const printing::JobEventDetails& event_details =
        *content::Details<printing::JobEventDetails>(details).ptr();
    switch (event_details.type()) {
      case printing::JobEventDetails::DOC_DONE:
        DCHECK(event_details.document());
        service_->JobIdGenerated(
            this, chromeos::CupsPrintJob::CreateUniqueId(
                      base::UTF16ToUTF8(
                          event_details.document()->settings().device_name()),
                      event_details.job_id()));
        break;
      case printing::JobEventDetails::FAILED:
        // TODO(vkuzkokov) see if we can extract an error message.
        JobError();
        break;
      default:
        // TODO(vkuzkokov) consider updating container on other events.
        break;
    }
  }

 private:
  void Destroy() { service_->DeleteJob(this); }

  base::OnceClosure MakeErrorHandler() {
    return base::BindOnce(&PrintJobHostImpl::Destroy,
                          weak_ptr_factory_.GetWeakPtr());
  }

  // Store Metafile and start printing if PrintJob is created as well.
  void OnFileRead(std::unique_ptr<printing::MetafileSkia> metafile) {
    metafile_ = std::move(metafile);
    StartPrintingIfReady();
  }

  // Create PrintJob and start printing if Metafile is created as well.
  void OnSetSettingsDone(scoped_refptr<printing::PrinterQuery> query) {
    job_ = base::MakeRefCounted<printing::PrintJob>();
    job_->Initialize(query.get(), base::string16() /* name */,
                     1 /* page_count */);
    registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                   content::Source<printing::PrintJob>(job_.get()));
    StartPrintingIfReady();
  }

  // If both PrintJob and Metafile are available start printing.
  void StartPrintingIfReady() {
    if (!job_ || !metafile_)
      return;

    printing::PrintedDocument* document = job_->document();
    document->SetDocument(std::move(metafile_) /* metafile */,
                          gfx::Size() /* paper_size */,
                          gfx::Rect() /* page_rect */);
    job_->StartPrinting();
  }

  // Binds the lifetime of |this| to the Mojo connection.
  mojo::Binding<mojom::PrintJobHost> binding_;

  mojom::PrintJobInstancePtr instance_;
  ArcPrintServiceImpl* const service_;
  chromeos::CupsPrintJobManager* const job_manager_;
  std::unique_ptr<printing::MetafileSkia> metafile_;
  scoped_refptr<printing::PrintJob> job_;
  chromeos::CupsPrintJob* cups_job_;
  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<PrintJobHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrintJobHostImpl);
};

ArcPrintServiceImpl::ArcPrintServiceImpl(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : profile_(Profile::FromBrowserContext(context)),
      arc_bridge_service_(bridge_service) {
  arc_bridge_service_->print()->SetHost(this);
  chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile_)
      ->AddObserver(this);
}

ArcPrintServiceImpl::~ArcPrintServiceImpl() {
  arc_bridge_service_->print()->SetHost(nullptr);
}

void ArcPrintServiceImpl::Shutdown() {
  chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile_)
      ->RemoveObserver(this);
}

void ArcPrintServiceImpl::OnPrintJobCreated(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  if (!job)
    return;
  auto it = jobs_by_id_.find(job->GetUniqueId());
  if (it != jobs_by_id_.end())
    it->second->CupsJobCreated(job.get());
}

void ArcPrintServiceImpl::OnPrintJobCancelled(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  if (!job)
    return;
  auto it = jobs_by_id_.find(job->GetUniqueId());
  if (it != jobs_by_id_.end())
    it->second->JobCanceled();
}

void ArcPrintServiceImpl::OnPrintJobError(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  if (!job)
    return;
  auto it = jobs_by_id_.find(job->GetUniqueId());
  if (it != jobs_by_id_.end())
    it->second->JobError();
}

void ArcPrintServiceImpl::OnPrintJobDone(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  if (!job)
    return;
  auto it = jobs_by_id_.find(job->GetUniqueId());
  if (it != jobs_by_id_.end())
    it->second->JobDone();
}

void ArcPrintServiceImpl::PrintDeprecated(mojo::ScopedHandle pdf_data) {
  LOG(ERROR) << "ArcPrintService::Print(ScopedHandle) is deprecated.";
}

void ArcPrintServiceImpl::Print(mojom::PrintJobInstancePtr instance,
                                mojom::PrintJobRequestPtr print_job,
                                PrintCallback callback) {
  instance->Start();

  const mojom::PrintAttributesPtr& attr = print_job->attributes;
  const mojom::PrintMediaSizePtr& arc_media = attr->media_size;
  const base::Optional<gfx::Size> resolution = attr->resolution;
  if (!arc_media || !resolution) {
    // TODO(vkuzkokov): localize
    instance->Fail(base::Optional<std::string>(
        base::in_place,
        "Print request must contain media size and resolution"));
    return;
  }

  const mojom::PrintMarginsPtr& margins = attr->min_margins;
  auto settings = std::make_unique<printing::PrintSettings>();

  gfx::Size size_mils(arc_media->width_mils, arc_media->height_mils);
  printing::PrintSettings::RequestedMedia media;
  media.size_microns =
      gfx::ScaleToRoundedSize(size_mils, printing::kMicronsPerMil);
  settings->set_requested_media(media);

  // TODO(vkuzkokov) Is it just max(dpm_hor, dpm_ver) as per
  // print_settings_conversion?
  float x_scale =
      static_cast<float>(resolution->width()) / printing::kMilsPerInch;
  float y_scale =
      static_cast<float>(resolution->height()) / printing::kMilsPerInch;
  settings->set_dpi_xy(resolution->width(), resolution->height());

  gfx::Rect area_mils(size_mils);
  if (margins) {
    area_mils.Inset(margins->left_mils, margins->top_mils, margins->right_mils,
                    margins->bottom_mils);
  }
  settings->SetPrinterPrintableArea(
      gfx::ScaleToRoundedSize(size_mils, x_scale, y_scale),
      gfx::ScaleToRoundedRect(area_mils, x_scale, y_scale), false);
  if (print_job->printer_id)
    settings->set_device_name(base::UTF8ToUTF16(print_job->printer_id.value()));

  // Chrome expects empty set of pages to mean "all".
  // Android uses a single range from 0 to 2^31-1 for that purpose.
  const printing::PageRanges& pages = print_job->pages;
  if (!pages.empty() && pages.back().to != std::numeric_limits<int>::max())
    settings->set_ranges(pages);

  settings->set_title(base::UTF8ToUTF16(print_job->document_name));
  settings->set_color(FromArcColorMode(attr->color_mode));
  settings->set_copies(print_job->copies);
  settings->set_duplex_mode(FromArcDuplexMode(attr->duplex_mode));

  base::ScopedFD fd =
      mojo::UnwrapPlatformHandle(std::move(print_job->data)).TakeFD();
  mojom::PrintJobHostPtr host_proxy;
  auto job = std::make_unique<PrintJobHostImpl>(
      mojo::MakeRequest(&host_proxy), std::move(instance), this,
      chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile_),
      std::move(settings), base::File(fd.release()), print_job->data_size);
  PrintJobHostImpl* job_raw = job.get();
  jobs_.emplace(job_raw, std::move(job));
  std::move(callback).Run(std::move(host_proxy));
}

void ArcPrintServiceImpl::CreateDiscoverySession(
    mojom::PrinterDiscoverySessionInstancePtr instance,
    CreateDiscoverySessionCallback callback) {
  mojom::PrinterDiscoverySessionHostPtr host_proxy;
  auto session = std::make_unique<PrinterDiscoverySessionHostImpl>(
      mojo::MakeRequest(&host_proxy), std::move(instance), this, profile_);
  PrinterDiscoverySessionHostImpl* session_raw = session.get();
  sessions_.emplace(session_raw, std::move(session));
  std::move(callback).Run(std::move(host_proxy));
}

void ArcPrintServiceImpl::DeleteJob(PrintJobHostImpl* job) {
  jobs_.erase(job);
}

void ArcPrintServiceImpl::DeleteSession(
    PrinterDiscoverySessionHostImpl* session) {
  sessions_.erase(session);
}

void ArcPrintServiceImpl::JobIdGenerated(PrintJobHostImpl* job,
                                         const std::string& job_id) {
  jobs_by_id_.emplace(job_id, job);
}

}  // namespace

// static
ArcPrintService* ArcPrintService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPrintServiceFactory::GetForBrowserContext(context);
}

ArcPrintService::ArcPrintService() {}

}  // namespace arc
