// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/local_printer_ash.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/print_server.h"
#include "chrome/browser/chromeos/printing/print_servers_manager.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_setup_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "chromeos/printing/ppd_provider.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/prefs_util.h"
#include "components/user_manager/user.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/printing_features.h"
#include "printing/printing_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace crosapi {

namespace {

class Observer : public chromeos::PrintServersManager::Observer {
 public:
  static void AddObserver(
      mojo::PendingRemote<mojom::PrintServerObserver> remote,
      chromeos::PrintServersManager* print_servers_manager) {
    Observer* observer = new Observer(std::move(remote), print_servers_manager);
    print_servers_manager->AddObserver(observer);
    observer->remote_.set_disconnect_handler(
        base::BindOnce(&Observer::RemoveObserver, base::Unretained(observer)));
  }

  void OnPrintServersChanged(
      const chromeos::PrintServersConfig& config) override {
    remote_->OnPrintServersChanged(LocalPrinterAsh::ConfigToMojom(config));
  }

  void OnServerPrintersChanged(
      const std::vector<chromeos::PrinterDetector::DetectedPrinter>&) override {
    remote_->OnServerPrintersChanged();
  }

 private:
  Observer(mojo::PendingRemote<mojom::PrintServerObserver> remote,
           chromeos::PrintServersManager* print_servers_manager)
      : remote_(std::move(remote)),
        print_servers_manager_(print_servers_manager) {}
  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;
  ~Observer() override = default;

  void RemoveObserver() {
    print_servers_manager_->RemoveObserver(this);
    delete this;
  }

  mojo::Remote<mojom::PrintServerObserver> remote_;
  chromeos::PrintServersManager* const print_servers_manager_;
};

// Generates and returns a url for a PPD license which is empty if
// an error occurs e.g. the ppd provider callback failed.
// When bound to a ppd provider scoped refptr, the reference count
// will be decremented once the callback is done executing and the
// ppd provider destroyed if it hits zero.
GURL GenerateEulaUrl(scoped_refptr<chromeos::PpdProvider>,
                     chromeos::PpdProvider::CallbackResultCode result,
                     const std::string& license) {
  if (result != chromeos::PpdProvider::CallbackResultCode::SUCCESS ||
      license.empty()) {
    return GURL();
  }
  return chromeos::PrinterConfigurer::GeneratePrinterEulaUrl(license);
}

// Destroys the PrinterConfigurer object once the callback has finished
// running.
mojom::CapabilitiesResponsePtr OnSetUpPrinter(
    std::unique_ptr<chromeos::PrinterConfigurer>,
    PrefService* prefs,
    const chromeos::Printer& printer,
    const absl::optional<printing::PrinterSemanticCapsAndDefaults>& caps) {
  return mojom::CapabilitiesResponse::New(
      LocalPrinterAsh::PrinterToMojom(printer), printer.HasSecureProtocol(),
      caps, prefs->GetInteger(prefs::kPrintingAllowedColorModes),
      prefs->GetInteger(prefs::kPrintingAllowedDuplexModes),
      static_cast<printing::mojom::PinModeRestriction>(
          prefs->GetInteger(prefs::kPrintingAllowedPinModes)),
      static_cast<printing::mojom::ColorModeRestriction>(
          prefs->GetInteger(prefs::kPrintingColorDefault)),
      static_cast<printing::mojom::DuplexModeRestriction>(
          prefs->GetInteger(prefs::kPrintingDuplexDefault)),
      static_cast<printing::mojom::PinModeRestriction>(
          prefs->GetInteger(prefs::kPrintingPinDefault)),
      0);  // deprecated
}

}  // namespace

LocalPrinterAsh::LocalPrinterAsh() = default;
LocalPrinterAsh::~LocalPrinterAsh() = default;

// static
mojom::PrintServersConfigPtr LocalPrinterAsh::ConfigToMojom(
    const chromeos::PrintServersConfig& config) {
  mojom::PrintServersConfigPtr ptr = mojom::PrintServersConfig::New();
  ptr->fetching_mode = config.fetching_mode;
  for (const chromeos::PrintServer& server : config.print_servers) {
    ptr->print_servers.push_back(mojom::PrintServer::New(
        server.GetId(), server.GetUrl(), server.GetName()));
  }
  return ptr;
}

// static
mojom::LocalDestinationInfoPtr LocalPrinterAsh::PrinterToMojom(
    const chromeos::Printer& printer) {
  return mojom::LocalDestinationInfo::New(
      printer.id(), printer.display_name(), printer.description(),
      printer.source() == chromeos::Printer::SRC_POLICY);
}

// static
mojom::PrinterStatusPtr LocalPrinterAsh::StatusToMojom(
    const chromeos::CupsPrinterStatus& status) {
  mojom::PrinterStatusPtr ptr = mojom::PrinterStatus::New();
  ptr->printer_id = status.GetPrinterId();
  ptr->timestamp = status.GetTimestamp();
  for (const auto& reason : status.GetStatusReasons()) {
    if (reason.GetReason() == mojom::StatusReason::Reason::kNoError)
      continue;
    ptr->status_reasons.push_back(
        mojom::StatusReason::New(reason.GetReason(), reason.GetSeverity()));
  }
  return ptr;
}

void LocalPrinterAsh::BindReceiver(
    mojo::PendingReceiver<mojom::LocalPrinter> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void LocalPrinterAsh::GetPrinters(GetPrintersCallback callback) {
  Profile* profile = GetActiveUserProfile();
  DCHECK(profile);
  // Printing is not allowed during OOBE.
  DCHECK(!chromeos::ProfileHelper::IsSigninProfile(profile));
  chromeos::CupsPrintersManager* printers_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile);
  std::vector<mojom::LocalDestinationInfoPtr> printers;
  for (chromeos::PrinterClass pc :
       {chromeos::PrinterClass::kSaved, chromeos::PrinterClass::kEnterprise,
        chromeos::PrinterClass::kAutomatic}) {
    for (const chromeos::Printer& p : printers_manager->GetPrinters(pc)) {
      VLOG(1) << "Found printer " << p.display_name() << " with device name "
              << p.id();
      printers.push_back(PrinterToMojom(p));
    }
  }
  std::move(callback).Run(std::move(printers));
}

void LocalPrinterAsh::GetCapability(const std::string& printer_id,
                                    GetCapabilityCallback callback) {
  Profile* profile = GetActiveUserProfile();
  DCHECK(profile);
  chromeos::CupsPrintersManager* printers_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile);
  absl::optional<chromeos::Printer> printer =
      printers_manager->GetPrinter(printer_id);
  if (!printer) {
    // If the printer was removed, the lookup will fail.
    std::move(callback).Run(nullptr);
    return;
  }
  std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer =
      CreatePrinterConfigurer(profile);
  chromeos::PrinterConfigurer* ptr = printer_configurer.get();
  printing::SetUpPrinter(
      printers_manager, ptr, *printer,
      base::BindOnce(OnSetUpPrinter, std::move(printer_configurer),
                     profile->GetPrefs(), *printer)
          .Then(std::move(callback)));
}

void LocalPrinterAsh::GetEulaUrl(const std::string& printer_id,
                                 GetEulaUrlCallback callback) {
  Profile* profile = GetActiveUserProfile();
  DCHECK(profile);
  chromeos::CupsPrintersManager* printers_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile);
  absl::optional<chromeos::Printer> printer =
      printers_manager->GetPrinter(printer_id);
  if (!printer) {
    // If the printer does not exist, fetching for the license will fail.
    std::move(callback).Run(GURL());
    return;
  }
  scoped_refptr<chromeos::PpdProvider> ppd_provider =
      CreatePpdProvider(profile);
  ppd_provider->ResolvePpdLicense(
      printer->ppd_reference().effective_make_and_model,
      base::BindOnce(GenerateEulaUrl, ppd_provider).Then(std::move(callback)));
}

void LocalPrinterAsh::GetStatus(const std::string& printer_id,
                                GetStatusCallback callback) {
  Profile* profile = GetActiveUserProfile();
  DCHECK(profile);
  chromeos::CupsPrintersManager* printers_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile);
  printers_manager->FetchPrinterStatus(
      printer_id, base::BindOnce(StatusToMojom).Then(std::move(callback)));
}

void LocalPrinterAsh::ShowSystemPrintSettings(
    ShowSystemPrintSettingsCallback callback) {
  Profile* profile = GetActiveUserProfile();
  DCHECK(profile);
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kPrintingDetailsSubpagePath);
  std::move(callback).Run();
}

void LocalPrinterAsh::CreatePrintJob(mojom::PrintJobPtr job,
                                     CreatePrintJobCallback callback) {
  Profile* profile = GetActiveUserProfile();
  DCHECK(profile);
  chromeos::CupsPrintJobManager* print_job_manager =
      chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
  chromeos::printing::proto::PrintSettings settings;
  settings.set_color(
      printing::IsColorModelSelected(job->color_mode)
          ? chromeos::printing::proto::PrintSettings_ColorMode_COLOR
          : chromeos::printing::proto::PrintSettings_ColorMode_BLACK_AND_WHITE);
  settings.set_duplex(
      static_cast<chromeos::printing::proto::PrintSettings_DuplexMode>(
          job->duplex_mode));
  settings.set_copies(job->copies);
  chromeos::printing::proto::MediaSize media_size;
  media_size.set_width(job->media_size.width());
  media_size.set_height(job->media_size.height());
  media_size.set_vendor_id(job->media_vendor_id);
  *settings.mutable_media_size() = media_size;
  print_job_manager->CreatePrintJob(job->device_name, job->title, job->job_id,
                                    job->page_count, job->source,
                                    job->source_id, std::move(settings));
  std::move(callback).Run();
}

void LocalPrinterAsh::GetPrintServersConfig(
    GetPrintServersConfigCallback callback) {
  Profile* profile = GetActiveUserProfile();
  DCHECK(profile);
  chromeos::PrintServersManager* print_servers_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile)
          ->GetPrintServersManager();
  std::move(callback).Run(
      ConfigToMojom(print_servers_manager->GetPrintServersConfig()));
}

void LocalPrinterAsh::ChoosePrintServers(
    const std::vector<std::string>& print_server_ids,
    ChoosePrintServersCallback callback) {
  Profile* profile = GetActiveUserProfile();
  DCHECK(profile);
  chromeos::PrintServersManager* print_servers_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile)
          ->GetPrintServersManager();
  print_servers_manager->ChoosePrintServer(print_server_ids);
  std::move(callback).Run();
}

void LocalPrinterAsh::AddPrintServerObserver(
    mojo::PendingRemote<mojom::PrintServerObserver> remote,
    AddPrintServerObserverCallback callback) {
  Profile* profile = GetActiveUserProfile();
  DCHECK(profile);
  chromeos::PrintServersManager* print_servers_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile)
          ->GetPrintServersManager();
  Observer::AddObserver(std::move(remote), print_servers_manager);
  std::move(callback).Run();
}

void LocalPrinterAsh::GetPolicies(GetPoliciesCallback callback) {
  Profile* profile = GetActiveUserProfile();
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
  if (prefs->HasPrefPath(prefs::kPrintingMaxSheetsAllowed)) {
    int max_sheets = prefs->GetInteger(prefs::kPrintingMaxSheetsAllowed);
    if (max_sheets >= 0) {
      policies->max_sheets_allowed = max_sheets;
      policies->max_sheets_allowed_has_value = true;
    }
  }
  std::move(callback).Run(std::move(policies));
}

void LocalPrinterAsh::GetUsernamePerPolicy(
    GetUsernamePerPolicyCallback callback) {
  Profile* profile = GetActiveUserProfile();
  const std::string username = chromeos::ProfileHelper::Get()
                                   ->GetUserByProfile(profile)
                                   ->display_email();
  std::move(callback).Run(profile->GetPrefs()->GetBoolean(
                              prefs::kPrintingSendUsernameAndFilenameEnabled)
                              ? absl::make_optional(username)
                              : absl::nullopt);
}

void LocalPrinterAsh::GetPrinterTypeDenyList(
    GetPrinterTypeDenyListCallback callback) {
  Profile* profile = GetActiveUserProfile();
  PrefService* prefs = profile->GetPrefs();

  std::vector<printing::mojom::PrinterType> deny_list;
  if (!prefs->HasPrefPath(prefs::kPrinterTypeDenyList)) {
    std::move(callback).Run(deny_list);
    return;
  }

  const base::Value* deny_list_from_prefs =
      prefs->Get(prefs::kPrinterTypeDenyList);
  if (!deny_list_from_prefs) {
    std::move(callback).Run(deny_list);
    return;
  }

  deny_list.reserve(deny_list_from_prefs->GetList().size());
  for (const base::Value& deny_list_value : deny_list_from_prefs->GetList()) {
    const std::string& deny_list_str = deny_list_value.GetString();
    printing::mojom::PrinterType printer_type;
    if (deny_list_str == "privet")
      printer_type = printing::mojom::PrinterType::kPrivet;
    else if (deny_list_str == "extension")
      printer_type = printing::mojom::PrinterType::kExtension;
    else if (deny_list_str == "pdf")
      printer_type = printing::mojom::PrinterType::kPdf;
    else if (deny_list_str == "local")
      printer_type = printing::mojom::PrinterType::kLocal;
    else if (deny_list_str == "cloud")
      printer_type = printing::mojom::PrinterType::kCloud;
    else
      continue;

    deny_list.push_back(printer_type);
  }
  std::move(callback).Run(deny_list);
}

Profile* LocalPrinterAsh::GetActiveUserProfile() {
  return ProfileManager::GetActiveUserProfile();
}

scoped_refptr<chromeos::PpdProvider> LocalPrinterAsh::CreatePpdProvider(
    Profile* profile) {
  return chromeos::CreatePpdProvider(profile);
}

std::unique_ptr<chromeos::PrinterConfigurer>
LocalPrinterAsh::CreatePrinterConfigurer(Profile* profile) {
  return chromeos::PrinterConfigurer::Create(profile);
}

}  // namespace crosapi
