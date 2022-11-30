// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/local_printer_ash.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
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
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/ash/printing/printer_setup_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/prefs_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/ppd_provider.h"
#include "components/prefs/pref_service.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace crosapi {

namespace {

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
  return ash::PrinterConfigurer::GeneratePrinterEulaUrl(license);
}

// Destroys the PrinterConfigurer object once the callback has finished
// running.
mojom::CapabilitiesResponsePtr OnSetUpPrinter(
    std::unique_ptr<ash::PrinterConfigurer>,
    PrefService* prefs,
    const chromeos::Printer& printer,
    const absl::optional<printing::PrinterSemanticCapsAndDefaults>& caps) {
  return mojom::CapabilitiesResponse::New(
      LocalPrinterAsh::PrinterToMojom(printer), printer.HasSecureProtocol(),
      caps,     // comment to prevent git cl format
      0, 0, 0,  // deprecated
      printing::mojom::PinModeRestriction::kUnset,     // deprecated
      printing::mojom::ColorModeRestriction::kUnset,   // deprecated
      printing::mojom::DuplexModeRestriction::kUnset,  // deprecated
      printing::mojom::PinModeRestriction::kUnset);    // deprecated
}

// This function is called when user's rights to access the printer were
// verified. The user can use the printer <=> `status` == StatusCode::kOK.
// Other values of `status` mean that the access was denied or an error
// occurred. The function is supposed to set-up the printer <=> the access was
// granted. The first parameter is used only for keep the pointer alive until
// this callback is executed.
void OnPrinterAuthenticated(
    std::unique_ptr<ash::printing::PrinterAuthenticator> /* authenticator */,
    std::unique_ptr<ash::PrinterConfigurer> printer_configurer,
    Profile* profile,
    ash::CupsPrintersManager* printers_manager,
    const chromeos::Printer& printer,
    mojom::LocalPrinter::GetCapabilityCallback callback,
    ash::printing::oauth2::StatusCode status,
    std::string /* access_token */) {
  if (status != ash::printing::oauth2::StatusCode::kOK) {
    // An error occurred.
    std::move(callback).Run(nullptr);
    return;
  }
  ash::PrinterConfigurer* ptr = printer_configurer.get();
  ash::printing::SetUpPrinter(
      printers_manager, ptr, printer,
      base::BindOnce(OnSetUpPrinter, std::move(printer_configurer),
                     profile->GetPrefs(), printer)
          .Then(std::move(callback)));
}

void OnOAuthAccessTokenObtained(
    std::unique_ptr<ash::printing::PrinterAuthenticator> /* authenticator */,
    mojom::LocalPrinter::GetOAuthAccessTokenCallback callback,
    ash::printing::oauth2::StatusCode status,
    std::string access_token) {
  if (status != ash::printing::oauth2::StatusCode::kOK) {
    // An error occurred.
    std::move(callback).Run(
        mojom::GetOAuthAccessTokenResult::NewError(mojom::OAuthError::New()));
    return;
  }
  if (access_token.empty()) {
    std::move(callback).Run(mojom::GetOAuthAccessTokenResult::NewNone(
        mojom::OAuthNotNeeded::New()));
  } else {
    std::move(callback).Run(mojom::GetOAuthAccessTokenResult::NewToken(
        mojom::OAuthAccessToken::New(std::move(access_token))));
  }
}

}  // namespace

LocalPrinterAsh::LocalPrinterAsh()
    : profile_manager_(g_browser_process->profile_manager()) {
  if (profile_manager_)
    profile_manager_->AddObserver(this);
}

LocalPrinterAsh::~LocalPrinterAsh() {
  if (profile_manager_)
    profile_manager_->RemoveObserver(this);
}

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

// static
mojom::LocalDestinationInfoPtr LocalPrinterAsh::PrinterToMojom(
    const chromeos::Printer& printer) {
  return mojom::LocalDestinationInfo::New(
      printer.id(), printer.display_name(), printer.description(),
      printer.source() == chromeos::Printer::SRC_POLICY,
      printer.uri().GetNormalized(/*always_print_port=*/true));
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
  profile_manager_->RemoveObserver(this);
  profile_manager_ = nullptr;
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
  for (auto& remote : print_job_remotes_)
    remote->OnPrintJobUpdate(job->printer().id(), job->job_id(), status);
  if (job->source() != mojom::PrintJob::Source::EXTENSION)
    return;
  for (auto& remote : extension_print_job_remotes_)
    remote->OnPrintJobUpdate(job->printer().id(), job->job_id(), status);
}

void LocalPrinterAsh::OnPrintServersChanged(
    const ash::PrintServersConfig& config) {
  for (auto& remote : print_server_remotes_)
    remote->OnPrintServersChanged(LocalPrinterAsh::ConfigToMojom(config));
}

void LocalPrinterAsh::OnServerPrintersChanged(
    const std::vector<ash::PrinterDetector::DetectedPrinter>&) {
  for (auto& remote : print_server_remotes_)
    remote->OnServerPrintersChanged();
}

void LocalPrinterAsh::GetPrinters(GetPrintersCallback callback) {
  Profile* profile = GetProfile();
  DCHECK(profile);
  // Printing is not allowed during OOBE.
  DCHECK(!ash::ProfileHelper::IsSigninProfile(profile));
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile);
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
  Profile* profile = GetProfile();
  DCHECK(profile);
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile);
  DCHECK(printers_manager);
  absl::optional<chromeos::Printer> printer =
      printers_manager->GetPrinter(printer_id);
  if (!printer) {
    // If the printer was removed, the lookup will fail.
    std::move(callback).Run(nullptr);
    return;
  }
  std::unique_ptr<ash::PrinterConfigurer> printer_configurer =
      CreatePrinterConfigurer(profile);

  if (ash::features::IsOAuthIppEnabled()) {
    ash::printing::oauth2::AuthorizationZonesManager* auth_manager =
        ash::printing::oauth2::AuthorizationZonesManagerFactory::
            GetForBrowserContext(profile);
    DCHECK(auth_manager);
    auto authenticator = std::make_unique<ash::printing::PrinterAuthenticator>(
        printers_manager, auth_manager, *printer);
    ash::printing::PrinterAuthenticator* authenticator_ptr =
        authenticator.get();
    authenticator_ptr->ObtainAccessTokenIfNeeded(
        base::BindOnce(OnPrinterAuthenticated, std::move(authenticator),
                       std::move(printer_configurer), profile, printers_manager,
                       *printer, std::move(callback)));
  } else {
    OnPrinterAuthenticated(nullptr, std::move(printer_configurer), profile,
                           printers_manager, *printer, std::move(callback),
                           ash::printing::oauth2::StatusCode::kOK, "");
  }
}

void LocalPrinterAsh::GetEulaUrl(const std::string& printer_id,
                                 GetEulaUrlCallback callback) {
  Profile* profile = GetProfile();
  DCHECK(profile);
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile);
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
  Profile* profile = GetProfile();
  DCHECK(profile);
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile);
  printers_manager->FetchPrinterStatus(
      printer_id, base::BindOnce(StatusToMojom).Then(std::move(callback)));
}

void LocalPrinterAsh::ShowSystemPrintSettings(
    ShowSystemPrintSettingsCallback callback) {
  Profile* profile = GetProfile();
  DCHECK(profile);
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kPrintingDetailsSubpagePath);
  std::move(callback).Run();
}

void LocalPrinterAsh::CreatePrintJob(mojom::PrintJobPtr job,
                                     CreatePrintJobCallback callback) {
  Profile* profile = GetProfile();
  DCHECK(profile);
  ash::CupsPrintJobManager* print_job_manager =
      ash::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
  ash::printing::proto::PrintSettings settings;
  settings.set_color(
      printing::IsColorModelSelected(job->color_mode)
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
  if (prefs->HasPrefPath(prefs::kPrintingMaxSheetsAllowed)) {
    int max_sheets = prefs->GetInteger(prefs::kPrintingMaxSheetsAllowed);
    if (max_sheets >= 0) {
      policies->max_sheets_allowed = max_sheets;
      policies->max_sheets_allowed_has_value = true;
    }
  }

  if (prefs->HasPrefPath(prefs::kPrintingAllowedColorModes))
    policies->allowed_color_modes =
        prefs->GetInteger(prefs::kPrintingAllowedColorModes);
  if (prefs->HasPrefPath(prefs::kPrintingAllowedDuplexModes))
    policies->allowed_duplex_modes =
        prefs->GetInteger(prefs::kPrintingAllowedDuplexModes);
  if (prefs->HasPrefPath(prefs::kPrintingAllowedPinModes))
    policies->allowed_pin_modes =
        static_cast<printing::mojom::PinModeRestriction>(
            prefs->GetInteger(prefs::kPrintingAllowedPinModes));
  if (prefs->HasPrefPath(prefs::kPrintingColorDefault))
    policies->default_color_mode =
        static_cast<printing::mojom::ColorModeRestriction>(
            prefs->GetInteger(prefs::kPrintingColorDefault));
  if (prefs->HasPrefPath(prefs::kPrintingDuplexDefault))
    policies->default_duplex_mode =
        static_cast<printing::mojom::DuplexModeRestriction>(
            prefs->GetInteger(prefs::kPrintingDuplexDefault));
  if (prefs->HasPrefPath(prefs::kPrintingPinDefault))
    policies->default_pin_mode =
        static_cast<printing::mojom::PinModeRestriction>(
            prefs->GetInteger(prefs::kPrintingPinDefault));

  if (prefs->HasPrefPath(prefs::kPrintPdfAsImageDefault)) {
    policies->default_print_pdf_as_image =
        prefs->GetBoolean(prefs::kPrintPdfAsImageDefault)
            ? mojom::Policies::OptionalBool::kTrue
            : mojom::Policies::OptionalBool::kFalse;
  }

  std::move(callback).Run(std::move(policies));
}

void LocalPrinterAsh::GetUsernamePerPolicy(
    GetUsernamePerPolicyCallback callback) {
  Profile* profile = GetProfile();
  const std::string username =
      ash::ProfileHelper::Get()->GetUserByProfile(profile)->display_email();
  std::move(callback).Run(profile->GetPrefs()->GetBoolean(
                              prefs::kPrintingSendUsernameAndFilenameEnabled)
                              ? absl::make_optional(username)
                              : absl::nullopt);
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
    if (deny_list_str == "extension")
      printer_type = printing::mojom::PrinterType::kExtension;
    else if (deny_list_str == "pdf")
      printer_type = printing::mojom::PrinterType::kPdf;
    else if (deny_list_str == "local")
      printer_type = printing::mojom::PrinterType::kLocal;
    else
      continue;

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
    case mojom::PrintJobSource::kAny:
      print_job_remotes_.Add(std::move(remote));
      break;
  }
  std::move(callback).Run();
}

void LocalPrinterAsh::GetOAuthAccessToken(
    const std::string& printer_id,
    GetOAuthAccessTokenCallback callback) {
  if (!ash::features::IsOAuthIppEnabled()) {
    std::move(callback).Run(mojom::GetOAuthAccessTokenResult::NewNone(
        mojom::OAuthNotNeeded::New()));
    return;
  }
  Profile* profile = GetProfile();
  DCHECK(profile);
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile);
  DCHECK(printers_manager);
  absl::optional<chromeos::Printer> printer =
      printers_manager->GetPrinter(printer_id);
  if (!printer) {
    // If the printer was removed, the lookup will fail.
    std::move(callback).Run(
        mojom::GetOAuthAccessTokenResult::NewError(mojom::OAuthError::New()));
    return;
  }
  ash::printing::oauth2::AuthorizationZonesManager* auth_manager =
      ash::printing::oauth2::AuthorizationZonesManagerFactory::
          GetForBrowserContext(profile);
  DCHECK(auth_manager);
  auto authenticator = std::make_unique<ash::printing::PrinterAuthenticator>(
      printers_manager, auth_manager, *printer);
  ash::printing::PrinterAuthenticator* authenticator_ptr = authenticator.get();
  authenticator_ptr->ObtainAccessTokenIfNeeded(
      base::BindOnce(OnOAuthAccessTokenObtained, std::move(authenticator),
                     std::move(callback)));
}

scoped_refptr<chromeos::PpdProvider> LocalPrinterAsh::CreatePpdProvider(
    Profile* profile) {
  return ash::CreatePpdProvider(profile);
}

std::unique_ptr<ash::PrinterConfigurer>
LocalPrinterAsh::CreatePrinterConfigurer(Profile* profile) {
  return ash::PrinterConfigurer::Create(profile);
}

}  // namespace crosapi
