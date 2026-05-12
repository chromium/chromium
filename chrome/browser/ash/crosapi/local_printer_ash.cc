// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/local_printer_ash.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/ash/printing/ppd_provider_factory.h"
#include "chrome/browser/ash/printing/print_management/printing_manager.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/ash/printing/print_server.h"
#include "chrome/browser/ash/printing/print_servers_manager.h"
#include "chrome/browser/ash/printing/printer_authenticator.h"
#include "chrome/browser/ash/printing/printer_setup_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/browser/printing/prefs_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/printing_features.h"
#include "printing/printing_utils.h"
#include "url/gurl.h"

namespace crosapi {

LocalPrinterAsh::LocalPrinterAsh() {
  auto* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    profile_manager_observer_.Observe(profile_manager);
  }
}

LocalPrinterAsh::~LocalPrinterAsh() = default;

// static
mojom::PrintServersConfigPtr LocalPrinterAsh::ConfigToMojom(
    const ash::PrintServersConfig& config) {
  mojom::PrintServersConfigPtr ptr = mojom::PrintServersConfig::New();
  ptr->fetching_mode = config.fetching_mode;
  for (const ash::PrintServer& server : config.print_servers) {
    ptr->print_servers.push_back(mojom::PrintServer::New(
        server.GetId(), server.GetUrl(), server.GetName()));
  }
  return ptr;
}

void LocalPrinterAsh::BindReceiver(
    mojo::PendingReceiver<mojom::LocalPrinter> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void LocalPrinterAsh::OnProfileAdded(Profile* profile) {
  if (observers_registered_ || !ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return;
  }

  auto* printers_manager_factory =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile);
  // In unit tests, `printers_manager_factory` can be null.
  if (!printers_manager_factory) {
    LOG(ERROR) << "CupsPrintersManagerFactory object not found";
    return;
  }
  observers_registered_ = true;
  // RemoveObserver() is not called since this object outlasts the
  // BrowserContextKeyedServices it's observing -
  // BrowserContextKeyedServices are destroyed in
  // ChromeBrowserMainParts::PostMainMessageLoopRun() while this object is
  // destroyed in ~ChromeBrowserMainParts().
  auto* print_servers_manager =
      printers_manager_factory->GetPrintServersManager();
  if (print_servers_manager) {
    print_servers_manager->AddObserver(this);
  } else {
    // This can occur during browser tests.
    LOG(ERROR) << "PrintServersManager object not found";
  }
  auto* print_job_manager =
      ash::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
  print_job_manager->AddObserver(this);
}

void LocalPrinterAsh::OnProfileManagerDestroying() {
  profile_manager_observer_.Reset();
}

void LocalPrinterAsh::OnPrintJobCreated(base::WeakPtr<ash::CupsPrintJob> job) {
  NotifyPrintJobUpdate(job, mojom::PrintJobStatus::kCreated);
}

void LocalPrinterAsh::OnPrintJobStarted(base::WeakPtr<ash::CupsPrintJob> job) {
  NotifyPrintJobUpdate(job, mojom::PrintJobStatus::kStarted);
}

void LocalPrinterAsh::OnPrintJobUpdated(base::WeakPtr<ash::CupsPrintJob> job) {
  NotifyPrintJobUpdate(job, mojom::PrintJobStatus::kUpdated);
}

void LocalPrinterAsh::OnPrintJobSuspended(
    base::WeakPtr<ash::CupsPrintJob> job) {
  NotifyPrintJobUpdate(job, mojom::PrintJobStatus::kSuspended);
}

void LocalPrinterAsh::OnPrintJobResumed(base::WeakPtr<ash::CupsPrintJob> job) {
  NotifyPrintJobUpdate(job, mojom::PrintJobStatus::kResumed);
}

void LocalPrinterAsh::OnPrintJobDone(base::WeakPtr<ash::CupsPrintJob> job) {
  NotifyPrintJobUpdate(job, mojom::PrintJobStatus::kDone);
}

void LocalPrinterAsh::OnPrintJobError(base::WeakPtr<ash::CupsPrintJob> job) {
  NotifyPrintJobUpdate(job, mojom::PrintJobStatus::kError);
}

void LocalPrinterAsh::OnPrintJobCancelled(
    base::WeakPtr<ash::CupsPrintJob> job) {
  NotifyPrintJobUpdate(job, mojom::PrintJobStatus::kCancelled);
}

void LocalPrinterAsh::NotifyPrintJobUpdate(base::WeakPtr<ash::CupsPrintJob> job,
                                           mojom::PrintJobStatus status) {
  if (!job) {
    LOG(WARNING) << "Ignoring invalid print job";
    return;
  }
  const auto& printer_id = job->printer().id();
  const auto& job_id = job->job_id();
  auto update = mojom::PrintJobUpdate::New();
  update->status = status;
  update->pages_printed = job->printed_page_number();
  for (auto& remote : print_job_remotes_) {
    remote->OnPrintJobUpdate(printer_id, job_id, update.Clone());
  }
  switch (job->source()) {
    case mojom::PrintJob::Source::kExtension:
      for (auto& remote : extension_print_job_remotes_) {
        remote->OnPrintJobUpdate(printer_id, job_id, update.Clone());
      }
      break;
    case mojom::PrintJob::Source::kIsolatedWebApp:
      for (auto& remote : iwa_print_job_remotes_) {
        remote->OnPrintJobUpdate(printer_id, job_id, update.Clone());
      }
      break;
    default:
      break;
  }
}

void LocalPrinterAsh::OnPrintServersChanged(
    const ash::PrintServersConfig& config) {
  for (auto& remote : print_server_remotes_) {
    remote->OnPrintServersChanged(LocalPrinterAsh::ConfigToMojom(config));
  }
}

void LocalPrinterAsh::OnServerPrintersChanged(
    const std::vector<ash::PrinterDetector::DetectedPrinter>&) {
  for (auto& remote : print_server_remotes_) {
    remote->OnServerPrintersChanged();
  }
}

void LocalPrinterAsh::CreatePrintJob(mojom::PrintJobPtr job,
                                     CreatePrintJobCallback callback) {
  Profile* profile = GetProfile();
  DCHECK(profile);
  ash::CupsPrintJobManager* print_job_manager =
      ash::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
  ash::printing::proto::PrintSettings settings;
  settings.set_color(
      printing::IsColorModelSelected(job->color_mode).value()
          ? ash::printing::proto::PrintSettings_ColorMode_COLOR
          : ash::printing::proto::PrintSettings_ColorMode_BLACK_AND_WHITE);
  settings.set_duplex(
      static_cast<ash::printing::proto::PrintSettings_DuplexMode>(
          job->duplex_mode));
  settings.set_copies(job->copies);
  ash::printing::proto::MediaSize media_size;
  media_size.set_width(job->media_size.width());
  media_size.set_height(job->media_size.height());
  media_size.set_vendor_id(job->media_vendor_id);
  *settings.mutable_media_size() = media_size;
  print_job_manager->CreatePrintJob(job->device_name, job->title, job->job_id,
                                    job->page_count, job->source,
                                    job->source_id, std::move(settings));
  std::move(callback).Run();
}

void LocalPrinterAsh::CancelPrintJob(const std::string& printer_id,
                                     unsigned int job_id,
                                     CancelPrintJobCallback callback) {
  Profile* profile = GetProfile();
  DCHECK(profile);
  ash::printing::print_management::PrintingManagerFactory::GetForProfile(
      profile)
      ->CancelPrintJob(ash::CupsPrintJob::CreateUniqueId(printer_id, job_id),
                       std::move(callback));
}

void LocalPrinterAsh::GetPrintServersConfig(
    GetPrintServersConfigCallback callback) {
  Profile* profile = GetProfile();
  DCHECK(profile);
  ash::PrintServersManager* print_servers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile)
          ->GetPrintServersManager();
  std::move(callback).Run(
      ConfigToMojom(print_servers_manager->GetPrintServersConfig()));
}

void LocalPrinterAsh::ChoosePrintServers(
    const std::vector<std::string>& print_server_ids,
    ChoosePrintServersCallback callback) {
  Profile* profile = GetProfile();
  DCHECK(profile);
  ash::PrintServersManager* print_servers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile)
          ->GetPrintServersManager();
  print_servers_manager->ChoosePrintServer(print_server_ids);
  std::move(callback).Run();
}

void LocalPrinterAsh::AddPrintServerObserver(
    mojo::PendingRemote<mojom::PrintServerObserver> remote,
    AddPrintServerObserverCallback callback) {
  print_server_remotes_.Add(std::move(remote));
  std::move(callback).Run();
}

void LocalPrinterAsh::GetPolicies(GetPoliciesCallback callback) {
  Profile* profile = GetProfile();
  PrefService* prefs = profile->GetPrefs();
  mojom::PoliciesPtr policies = mojom::Policies::New();

  if (prefs->HasPrefPath(prefs::kPrintHeaderFooter)) {
    (prefs->IsManagedPreference(prefs::kPrintHeaderFooter)
         ? policies->print_header_footer_allowed
         : policies->print_header_footer_default) =
        prefs->GetBoolean(prefs::kPrintHeaderFooter)
            ? mojom::Policies::OptionalBool::kTrue
            : mojom::Policies::OptionalBool::kFalse;
  }

  if (prefs->HasPrefPath(prefs::kPrintingAllowedBackgroundGraphicsModes)) {
    policies->allowed_background_graphics_modes =
        static_cast<mojom::Policies::BackgroundGraphicsModeRestriction>(
            prefs->GetInteger(prefs::kPrintingAllowedBackgroundGraphicsModes));
  }
  if (prefs->HasPrefPath(prefs::kPrintingBackgroundGraphicsDefault)) {
    policies->background_graphics_default =
        static_cast<mojom::Policies::BackgroundGraphicsModeRestriction>(
            prefs->GetInteger(prefs::kPrintingBackgroundGraphicsDefault));
  }

  policies->paper_size_default = printing::ParsePaperSizeDefault(*prefs);
  if (prefs->HasPrefPath(ash::prefs::kPrintingMaxSheetsAllowed)) {
    int max_sheets = prefs->GetInteger(ash::prefs::kPrintingMaxSheetsAllowed);
    if (max_sheets >= 0) {
      policies->max_sheets_allowed = max_sheets;
      policies->max_sheets_allowed_has_value = true;
    }
  }

  if (prefs->HasPrefPath(ash::prefs::kPrintingAllowedColorModes)) {
    policies->allowed_color_modes =
        prefs->GetInteger(ash::prefs::kPrintingAllowedColorModes);
  }
  if (prefs->HasPrefPath(ash::prefs::kPrintingAllowedDuplexModes)) {
    policies->allowed_duplex_modes =
        prefs->GetInteger(ash::prefs::kPrintingAllowedDuplexModes);
  }
  if (prefs->HasPrefPath(ash::prefs::kPrintingAllowedPinModes)) {
    policies->allowed_pin_modes =
        static_cast<printing::mojom::PinModeRestriction>(
            prefs->GetInteger(ash::prefs::kPrintingAllowedPinModes));
  }
  if (prefs->HasPrefPath(ash::prefs::kPrintingColorDefault)) {
    policies->default_color_mode =
        static_cast<printing::mojom::ColorModeRestriction>(
            prefs->GetInteger(ash::prefs::kPrintingColorDefault));
  }
  if (prefs->HasPrefPath(ash::prefs::kPrintingDuplexDefault)) {
    policies->default_duplex_mode =
        static_cast<printing::mojom::DuplexModeRestriction>(
            prefs->GetInteger(ash::prefs::kPrintingDuplexDefault));
  }
  if (prefs->HasPrefPath(ash::prefs::kPrintingPinDefault)) {
    policies->default_pin_mode =
        static_cast<printing::mojom::PinModeRestriction>(
            prefs->GetInteger(ash::prefs::kPrintingPinDefault));
  }

  if (prefs->HasPrefPath(prefs::kPrintPdfAsImageDefault)) {
    policies->default_print_pdf_as_image =
        prefs->GetBoolean(prefs::kPrintPdfAsImageDefault)
            ? mojom::Policies::OptionalBool::kTrue
            : mojom::Policies::OptionalBool::kFalse;
  }

  std::move(callback).Run(std::move(policies));
}


void LocalPrinterAsh::GetPrinterTypeDenyList(
    GetPrinterTypeDenyListCallback callback) {
  Profile* profile = GetProfile();
  PrefService* prefs = profile->GetPrefs();

  std::vector<printing::mojom::PrinterType> deny_list;
  if (!prefs->HasPrefPath(prefs::kPrinterTypeDenyList)) {
    std::move(callback).Run(deny_list);
    return;
  }

  const base::Value& deny_list_from_prefs =
      prefs->GetValue(prefs::kPrinterTypeDenyList);

  deny_list.reserve(deny_list_from_prefs.GetList().size());
  for (const base::Value& deny_list_value : deny_list_from_prefs.GetList()) {
    const std::string& deny_list_str = deny_list_value.GetString();
    printing::mojom::PrinterType printer_type;
    if (deny_list_str == "extension") {
      printer_type = printing::mojom::PrinterType::kExtension;
    } else if (deny_list_str == "pdf") {
      printer_type = printing::mojom::PrinterType::kPdf;
    } else if (deny_list_str == "local") {
      printer_type = printing::mojom::PrinterType::kLocal;
    } else {
      continue;
    }

    deny_list.push_back(printer_type);
  }
  std::move(callback).Run(deny_list);
}

Profile* LocalPrinterAsh::GetProfile() {
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return nullptr;
  }
  return ProfileManager::GetPrimaryUserProfile();
}

void LocalPrinterAsh::AddPrintJobObserver(
    mojo::PendingRemote<mojom::PrintJobObserver> remote,
    mojom::PrintJobSource source,
    AddPrintJobObserverCallback callback) {
  switch (source) {
    case mojom::PrintJobSource::kExtension:
      extension_print_job_remotes_.Add(std::move(remote));
      break;
    case mojom::PrintJobSource::kIsolatedWebApp:
      iwa_print_job_remotes_.Add(std::move(remote));
      break;
    case mojom::PrintJobSource::kAny:
      print_job_remotes_.Add(std::move(remote));
      break;
  }
  std::move(callback).Run();
}

scoped_refptr<chromeos::PpdProvider> LocalPrinterAsh::CreatePpdProvider(
    Profile* profile) {
  return ash::CreatePpdProvider(profile);
}


}  // namespace crosapi
