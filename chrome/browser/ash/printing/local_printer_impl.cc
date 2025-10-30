// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/local_printer_impl.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/printer_authenticator.h"
#include "chrome/browser/ash/printing/printer_setup_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"

namespace ash {

namespace {

LocalPrinter* g_instance = nullptr;

std::vector<chromeos::Printer> GetLocalPrinters(const AccountId& accountId) {
  static constexpr chromeos::PrinterClass printer_classes_to_fetch[] = {
      chromeos::PrinterClass::kSaved, chromeos::PrinterClass::kEnterprise,
      chromeos::PrinterClass::kAutomatic, chromeos::PrinterClass::kDiscovered};
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(
          ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
              accountId));
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

// Mark if a not yet installed printer is autoconf then continue with setup.
void OnPrinterQueriedForAutoConf(ash::CupsPrintersManager* printers_manager,
                                 LocalPrinter::GetCapabilityCallback callback,
                                 chromeos::Printer printer,
                                 bool is_printer_autoconf,
                                 const chromeos::IppPrinterInfo& info) {
  if (!is_printer_autoconf) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  printer.mutable_ppd_reference()->autoconf = true;
  printer.set_ipp_printer_info(info);
  printing::SetUpPrinter(printers_manager, printer, std::move(callback));
}

// Query the printer for setup metrics then continue with setup.
void OnPrinterQueriedForAutoConfMetricsOnly(
    ash::CupsPrintersManager* printers_manager,
    LocalPrinter::GetCapabilityCallback callback,
    chromeos::Printer printer,
    bool is_printer_autoconf,
    const chromeos::IppPrinterInfo& info) {
  printer.set_ipp_printer_info(info);
  printing::SetUpPrinter(printers_manager, printer, std::move(callback));
}

// This function is called when user's rights to access the printer were
// verified. The user can use the printer <=> `status` == StatusCode::kOK.
// Other values of `status` mean that the access was denied or an error
// occurred. The function is supposed to set-up the printer <=> the access was
// granted. The first parameter is used only for keep the pointer alive until
// this callback is executed.
void OnPrinterAuthenticated(
    std::unique_ptr<ash::printing::PrinterAuthenticator> /* authenticator */,
    ash::CupsPrintersManager* printers_manager,
    const chromeos::Printer& printer,
    LocalPrinter::GetCapabilityCallback callback,
    ash::printing::oauth2::StatusCode status,
    std::string /* access_token */) {
  if (status != ash::printing::oauth2::StatusCode::kOK) {
    // An error occurred.
    std::move(callback).Run(std::nullopt);
    return;
  }

  // In order for an IPP printer to be valid for set up, it needs to either be
  // previously installed, be autoconf compatible, or have a valid PPD
  // reference. If necessary, the printer is queried to determine its autoconf
  // compatibility.
  if (!printers_manager->IsPrinterInstalled(printer)) {
    if (!printer.HasUri()) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    // If the printer is autoconf compatible or has a valid PPD reference then
    // continue with normal setup.
    if (printer.ppd_reference().IsFilled()) {
      printers_manager->QueryPrinterForAutoConf(
          printer,
          base::BindOnce(OnPrinterQueriedForAutoConfMetricsOnly,
                         printers_manager, std::move(callback), printer));
      return;
    }

    // CupsPrintersManager should have marked compatible USB printers as having
    // a valid PPD reference or autoconf, so this USB printer is incompatible.
    if (printer.IsUsbProtocol()) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    printers_manager->QueryPrinterForAutoConf(
        printer, base::BindOnce(OnPrinterQueriedForAutoConf, printers_manager,
                                std::move(callback), printer));
    return;
  }

  printers_manager->QueryPrinterForAutoConf(
      printer, base::BindOnce(OnPrinterQueriedForAutoConfMetricsOnly,
                              printers_manager, std::move(callback), printer));
}

}  // namespace

LocalPrinter* LocalPrinterImpl::Get() {
  CHECK(g_instance);
  return g_instance;
}

LocalPrinterImpl::LocalPrinterImpl() {
  CHECK(!g_instance);
  g_instance = this;
}

LocalPrinterImpl::~LocalPrinterImpl() {
  g_instance = nullptr;
}

void LocalPrinterImpl::GetPrinters(const AccountId& accountId,
                                   LocalPrinter::GetPrintersCallback callback) {
  std::move(callback).Run(GetLocalPrinters(accountId));
}

void LocalPrinterImpl::GetCapability(
    const AccountId& accountId,
    const std::string& printer_id,
    LocalPrinter::GetCapabilityCallback callback) {
  content::BrowserContext* context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(accountId);
  ash::CupsPrintersManager* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(context);
  DCHECK(printers_manager);
  std::optional<chromeos::Printer> printer =
      printers_manager->GetPrinter(printer_id);
  if (!printer) {
    // If the printer was removed, the lookup will fail.
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (ash::features::IsOAuthIppEnabled()) {
    ash::printing::oauth2::AuthorizationZonesManager* auth_manager =
        ash::printing::oauth2::AuthorizationZonesManagerFactory::
            GetForBrowserContext(context);
    DCHECK(auth_manager);
    auto authenticator = std::make_unique<ash::printing::PrinterAuthenticator>(
        printers_manager, auth_manager, *printer);
    ash::printing::PrinterAuthenticator* authenticator_ptr =
        authenticator.get();
    authenticator_ptr->ObtainAccessTokenIfNeeded(
        base::BindOnce(OnPrinterAuthenticated, std::move(authenticator),
                       printers_manager, *printer, std::move(callback)));
  } else {
    OnPrinterAuthenticated(nullptr, printers_manager, *printer,
                           std::move(callback),
                           ash::printing::oauth2::StatusCode::kOK, "");
  }
}

}  // namespace ash
