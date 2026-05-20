// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/local_printer_impl.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/logging.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/ppd_provider_factory.h"
#include "chrome/browser/ash/printing/printer_authenticator.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/ash/printing/printer_setup_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/printing/ppd_provider.h"
#include "components/account_id/account_id.h"
#include "url/gurl.h"

namespace ash {

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

std::vector<chromeos::Printer> GetLocalPrinters(const AccountId& accountId) {
  static constexpr chromeos::PrinterClass printer_classes_to_fetch[] = {
      chromeos::PrinterClass::kSaved, chromeos::PrinterClass::kEnterprise,
      chromeos::PrinterClass::kAutomatic, chromeos::PrinterClass::kDiscovered};
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(
          ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
              accountId));
  CHECK(printers_manager);
  std::vector<chromeos::Printer> printers;
  for (chromeos::PrinterClass pc : printer_classes_to_fetch) {
    for (const chromeos::Printer& p : printers_manager->GetPrinters(pc)) {
      VLOG(1) << "Found printer " << p.display_name() << " with device name "
              << p.id();
      printers.push_back(p);
    }
  }

  return printers;
}

void OnSetUpPrinter(
    LocalPrinter::GetCapabilityCallback callback,
    const chromeos::Printer& printer,
    const std::optional<::printing::PrinterSemanticCapsAndDefaults>& caps) {
  std::move(callback).Run(base::optional_ref<const chromeos::Printer>(printer),
                          caps);
}

// Mark if a not yet installed printer is autoconf then continue with setup.
void OnPrinterQueriedForAutoConf(
    const ApplicationLocaleStorage* application_locale_storage,
    ash::CupsPrintersManager* printers_manager,
    LocalPrinter::GetCapabilityCallback callback,
    chromeos::Printer printer,
    bool is_printer_autoconf,
    const chromeos::IppPrinterInfo& info) {
  if (!is_printer_autoconf) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }

  printer.mutable_ppd_reference()->autoconf = true;
  printer.set_ipp_printer_info(info);
  printing::SetUpPrinter(
      application_locale_storage, printers_manager, printer,
      base::BindOnce(OnSetUpPrinter, std::move(callback), printer));
}

// Query the printer for setup metrics then continue with setup.
void OnPrinterQueriedForAutoConfMetricsOnly(
    const ApplicationLocaleStorage* application_locale_storage,
    ash::CupsPrintersManager* printers_manager,
    LocalPrinter::GetCapabilityCallback callback,
    chromeos::Printer printer,
    bool is_printer_autoconf,
    const chromeos::IppPrinterInfo& info) {
  printer.set_ipp_printer_info(info);
  printing::SetUpPrinter(
      application_locale_storage, printers_manager, printer,
      base::BindOnce(OnSetUpPrinter, std::move(callback), printer));
}

// This function is called when user's rights to access the printer were
// verified. The user can use the printer <=> `status` == StatusCode::kOK.
// Other values of `status` mean that the access was denied or an error
// occurred. The function is supposed to set-up the printer <=> the access was
// granted. `application_locale_storage` must be non-null and remain valid while
// the main RunLoop is running. The second parameter is used only
// to keep the pointer alive until this callback is executed.
void OnPrinterAuthenticated(
    const ApplicationLocaleStorage* application_locale_storage,
    std::unique_ptr<ash::printing::PrinterAuthenticator> /* authenticator */,
    ash::CupsPrintersManager* printers_manager,
    const chromeos::Printer& printer,
    LocalPrinter::GetCapabilityCallback callback,
    ash::printing::oauth2::StatusCode status,
    std::string /* access_token */) {
  if (status != ash::printing::oauth2::StatusCode::kOK) {
    // An error occurred.
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }

  // In order for an IPP printer to be valid for set up, it needs to either be
  // previously installed, be autoconf compatible, or have a valid PPD
  // reference. If necessary, the printer is queried to determine its autoconf
  // compatibility.
  if (!printers_manager->IsPrinterInstalled(printer)) {
    if (!printer.HasUri()) {
      std::move(callback).Run(std::nullopt, std::nullopt);
      return;
    }

    // If the printer is autoconf compatible or has a valid PPD reference then
    // continue with normal setup.
    if (printer.ppd_reference().IsFilled()) {
      printers_manager->QueryPrinterForAutoConf(
          printer, base::BindOnce(OnPrinterQueriedForAutoConfMetricsOnly,
                                  application_locale_storage, printers_manager,
                                  std::move(callback), printer));
      return;
    }

    // CupsPrintersManager should have marked compatible USB printers as having
    // a valid PPD reference or autoconf, so this USB printer is incompatible.
    if (printer.IsUsbProtocol()) {
      std::move(callback).Run(std::nullopt, std::nullopt);
      return;
    }

    printers_manager->QueryPrinterForAutoConf(
        printer,
        base::BindOnce(OnPrinterQueriedForAutoConf, application_locale_storage,
                       printers_manager, std::move(callback), printer));
    return;
  }

  printers_manager->QueryPrinterForAutoConf(
      printer, base::BindOnce(OnPrinterQueriedForAutoConfMetricsOnly,
                              application_locale_storage, printers_manager,
                              std::move(callback), printer));
}

void OnOAuthAccessTokenObtained(
    std::unique_ptr<ash::printing::PrinterAuthenticator> authenticator,
    LocalPrinter::GetOAuthAccessTokenCallback callback,
    ash::printing::oauth2::StatusCode status,
    std::string access_token) {
  if (status != ash::printing::oauth2::StatusCode::kOK) {
    LOG(ERROR) << "Failed to obtain access token: " << static_cast<int>(status);
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (access_token.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(access_token);
}

}  // namespace

LocalPrinterImpl::LocalPrinterImpl(
    const ApplicationLocaleStorage* application_locale_storage)
    : application_locale_storage_(CHECK_DEREF(application_locale_storage)) {}

LocalPrinterImpl::~LocalPrinterImpl() = default;

void LocalPrinterImpl::GetPrinters(const AccountId& accountId,
                                   LocalPrinter::GetPrintersCallback callback) {
  std::move(callback).Run(GetLocalPrinters(accountId));
}

std::optional<chromeos::Printer> LocalPrinterImpl::GetPrinter(
    const AccountId& accountId,
    const std::string& printer_id) {
  content::BrowserContext* context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(accountId);
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(context);
  CHECK(printers_manager);
  return printers_manager->GetPrinter(printer_id);
}

void LocalPrinterImpl::GetCapability(
    const AccountId& accountId,
    const std::string& printer_id,
    LocalPrinter::GetCapabilityCallback callback) {
  content::BrowserContext* context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(accountId);
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(context);
  CHECK(printers_manager);
  std::optional<chromeos::Printer> printer =
      printers_manager->GetPrinter(printer_id);
  if (!printer) {
    // If the printer was removed, the lookup will fail.
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }

  if (ash::features::IsOAuthIppEnabled()) {
    ash::printing::oauth2::AuthorizationZonesManager* auth_manager =
        ash::printing::oauth2::AuthorizationZonesManagerFactory::
            GetForBrowserContext(context);
    CHECK(auth_manager);
    auto authenticator = std::make_unique<ash::printing::PrinterAuthenticator>(
        printers_manager, auth_manager, *printer);
    ash::printing::PrinterAuthenticator* authenticator_ptr =
        authenticator.get();
    authenticator_ptr->ObtainAccessTokenIfNeeded(base::BindOnce(
        OnPrinterAuthenticated, &application_locale_storage_.get(),
        std::move(authenticator), printers_manager, *printer,
        std::move(callback)));
  } else {
    OnPrinterAuthenticated(&application_locale_storage_.get(), nullptr,
                           printers_manager, *printer, std::move(callback),
                           ash::printing::oauth2::StatusCode::kOK, "");
  }
}

void LocalPrinterImpl::GetEulaUrl(const AccountId& accountId,
                                  const std::string& printer_id,
                                  LocalPrinter::GetEulaUrlCallback callback) {
  content::BrowserContext* context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(accountId);
  Profile* profile = Profile::FromBrowserContext(context);
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(context);
  CHECK(printers_manager);
  std::optional<chromeos::Printer> printer =
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

void LocalPrinterImpl::GetStatus(const AccountId& accountId,
                                 const std::string& printer_id,
                                 LocalPrinter::GetStatusCallback callback) {
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(
          ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
              accountId));
  CHECK(printers_manager);
  printers_manager->FetchPrinterStatus(printer_id, std::move(callback));
}

void LocalPrinterImpl::GetOAuthAccessToken(
    const AccountId& accountId,
    const std::string& printer_id,
    LocalPrinter::GetOAuthAccessTokenCallback callback) {
  if (!ash::features::IsOAuthIppEnabled()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  auto* browser_context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(accountId);
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(browser_context);
  CHECK(printers_manager);
  std::optional<chromeos::Printer> printer =
      printers_manager->GetPrinter(printer_id);
  if (!printer) {
    // If the printer does not exist, fetching for the license will fail.
    LOG(ERROR) << "Printer " << printer_id << " not found";
    std::move(callback).Run(std::nullopt);
    return;
  }
  ash::printing::oauth2::AuthorizationZonesManager* auth_manager =
      ash::printing::oauth2::AuthorizationZonesManagerFactory::
          GetForBrowserContext(browser_context);
  CHECK(auth_manager);
  auto authenticator = std::make_unique<ash::printing::PrinterAuthenticator>(
      printers_manager, auth_manager, *printer);
  ash::printing::PrinterAuthenticator* authenticator_ptr = authenticator.get();
  authenticator_ptr->ObtainAccessTokenIfNeeded(
      base::BindOnce(OnOAuthAccessTokenObtained, std::move(authenticator),
                     std::move(callback)));
}

scoped_refptr<chromeos::PpdProvider> LocalPrinterImpl::CreatePpdProvider(
    Profile* profile) {
  return ash::CreatePpdProvider(profile);
}

}  // namespace ash
