// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_configurer.h"

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"
#include "chromeos/dbus/common/dbus_library_error.h"
#include "chromeos/printing/ppd_line_reader.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"
#include "printing/printing_features.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace ash {

namespace {

using ::chromeos::PpdProvider;
using ::chromeos::Printer;

const char kEbuildWithHplipPlugins[] = "hplip-plugin";

PrinterSetupResult PrinterSetupResultFromDbusResultCode(const Printer& printer,
                                                        int result_code) {
  DCHECK_GE(result_code, 0);
  const std::string prefix =
      printer.id() + ": " + printer.make_and_model() + " setup result: ";
  switch (result_code) {
    case debugd::CupsResult::CUPS_SUCCESS:
      PRINTER_LOG(EVENT) << prefix << "Printer setup successful";
      return PrinterSetupResult::kSuccess;
    case debugd::CupsResult::CUPS_INVALID_PPD:
      PRINTER_LOG(EVENT) << prefix << "PPD Invalid";
      return PrinterSetupResult::kInvalidPpd;
    case debugd::CupsResult::CUPS_LPADMIN_FAILURE:
      PRINTER_LOG(ERROR) << prefix << "lpadmin-manual failed";
      return PrinterSetupResult::kFatalError;
    case debugd::CupsResult::CUPS_AUTOCONF_FAILURE:
      PRINTER_LOG(ERROR) << prefix << "lpadmin-autoconf failed";
      return PrinterSetupResult::kFatalError;
    case debugd::CupsResult::CUPS_BAD_URI:
      PRINTER_LOG(EVENT) << prefix << "Bad URI";
      return PrinterSetupResult::kBadUri;
    case debugd::CupsResult::CUPS_IO_ERROR:
      PRINTER_LOG(ERROR) << prefix << "I/O error";
      return PrinterSetupResult::kIoError;
    case debugd::CupsResult::CUPS_MEMORY_ALLOC_ERROR:
      PRINTER_LOG(EVENT) << prefix << "Memory allocation error";
      return PrinterSetupResult::kMemoryAllocationError;
    case debugd::CupsResult::CUPS_PRINTER_UNREACHABLE:
      PRINTER_LOG(EVENT) << prefix << "Printer is unreachable";
      return PrinterSetupResult::kPrinterUnreachable;
    case debugd::CupsResult::CUPS_PRINTER_WRONG_RESPONSE:
      PRINTER_LOG(EVENT) << prefix << "Unexpected response from printer";
      return PrinterSetupResult::kPrinterSentWrongResponse;
    case debugd::CupsResult::CUPS_PRINTER_NOT_AUTOCONF:
      PRINTER_LOG(EVENT) << prefix << "Printer is not autoconfigurable";
      return PrinterSetupResult::kPrinterIsNotAutoconfigurable;
    case debugd::CupsResult::CUPS_FATAL:
    default:
      // We have no idea.  It must be fatal.
      PRINTER_LOG(ERROR) << prefix << "Unrecognized error: " << result_code;
      return PrinterSetupResult::kFatalError;
  }
}

// Map D-Bus errors from the debug daemon client to D-Bus errors enumerated
// in PrinterSetupResult.
PrinterSetupResult PrinterSetupResultFromDbusErrorCode(
    const Printer& printer,
    chromeos::DBusLibraryError dbus_error) {
  DCHECK_LT(dbus_error, 0);
  const std::string prefix =
      printer.id() + ": " + printer.make_and_model() + " setup result: ";
  switch (dbus_error) {
    case chromeos::DBusLibraryError::kNoReply:
      PRINTER_LOG(ERROR) << prefix << "D-Bus error - no reply";
      return PrinterSetupResult::kDebugdDbusNoReply;
    case chromeos::DBusLibraryError::kTimeout:
      PRINTER_LOG(ERROR) << prefix << "D-Bus error - timeout";
      return PrinterSetupResult::kDbusTimeout;
    default:
      PRINTER_LOG(ERROR) << prefix << "Unknown D-Bus error";
      return PrinterSetupResult::kDbusError;
  }
}

PrinterSetupResult PrinterSetupResultFromAddPrinterResult(
    const Printer& printer,
    printscanmgr::AddPrinterResult result) {
  const std::string prefix =
      printer.id() + ":" + printer.make_and_model() + " setup result: ";
  switch (result) {
    case printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_SUCCESS:
      PRINTER_LOG(EVENT) << prefix << "Printer setup successful";
      return PrinterSetupResult::kSuccess;
    case printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_CUPS_INVALID_PPD:
      PRINTER_LOG(EVENT) << prefix << "PPD Invalid";
      return PrinterSetupResult::kInvalidPpd;
    case printscanmgr::AddPrinterResult::
        ADD_PRINTER_RESULT_CUPS_LPADMIN_FAILURE:
      PRINTER_LOG(ERROR) << prefix << "lpadmin-manual failed";
      return PrinterSetupResult::kFatalError;
    case printscanmgr::AddPrinterResult::
        ADD_PRINTER_RESULT_CUPS_AUTOCONF_FAILURE:
      PRINTER_LOG(ERROR) << prefix << "lpadmin-autoconf failed";
      return PrinterSetupResult::kFatalError;
    case printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_CUPS_BAD_URI:
      PRINTER_LOG(EVENT) << prefix << "Bad URI";
      return PrinterSetupResult::kBadUri;
    case printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_CUPS_IO_ERROR:
      PRINTER_LOG(ERROR) << prefix << "I/O error";
      return PrinterSetupResult::kIoError;
    case printscanmgr::AddPrinterResult::
        ADD_PRINTER_RESULT_CUPS_MEMORY_ALLOC_ERROR:
      PRINTER_LOG(EVENT) << prefix << "Memory allocation error";
      return PrinterSetupResult::kMemoryAllocationError;
    case printscanmgr::AddPrinterResult::
        ADD_PRINTER_RESULT_CUPS_PRINTER_UNREACHABLE:
      PRINTER_LOG(EVENT) << prefix << "Printer is unreachable";
      return PrinterSetupResult::kPrinterUnreachable;
    case printscanmgr::AddPrinterResult::
        ADD_PRINTER_RESULT_CUPS_PRINTER_WRONG_RESPONSE:
      PRINTER_LOG(EVENT) << prefix << "Unexpected response from printer";
      return PrinterSetupResult::kPrinterSentWrongResponse;
    case printscanmgr::AddPrinterResult::
        ADD_PRINTER_RESULT_CUPS_PRINTER_NOT_AUTOCONF:
      PRINTER_LOG(EVENT) << prefix << "Printer is not autoconfigurable";
      return PrinterSetupResult::kPrinterIsNotAutoconfigurable;
    case printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_GENERIC:
      PRINTER_LOG(ERROR) << prefix << "Unknown D-Bus error";
      return PrinterSetupResult::kDbusError;
    case printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_NO_REPLY:
      PRINTER_LOG(ERROR) << prefix << "D-Bus error - no reply";
      return PrinterSetupResult::kPrintscanmgrDbusNoReply;
    case printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_TIMEOUT:
      PRINTER_LOG(ERROR) << prefix << "D-Bus error - timeout";
      return PrinterSetupResult::kDbusTimeout;
    // TODO(pmoy): handle new D-Bus encoding error here.
    case printscanmgr::AddPrinterResult::
        ADD_PRINTER_RESULT_UNSPECIFIED:  // FALLTHROUGH
    case printscanmgr::AddPrinterResult::
        ADD_PRINTER_RESULT_CUPS_FATAL:  // FALLTHROUGH
    default:
      // We have no idea.  It must be fatal.
      PRINTER_LOG(ERROR) << prefix << "Unrecognized error: "
                         << printscanmgr::AddPrinterResult_Name(result);
      return PrinterSetupResult::kFatalError;
  }
}

// Searches in `ppd` for a command setting HP printer language. If the keyword
// is found, the function inserts after the command a line containing a path to
// Hplip plugin provided in `path` and returns true.
bool AddHplipPluginPathToPpdContent(std::string_view path, std::string& ppd) {
  constexpr char kHpPrinterLanguageKeyword[] = "*hpPrinterLanguage:";
  size_t pos = ppd.find(kHpPrinterLanguageKeyword);
  if (pos != std::string::npos) {
    pos = ppd.find('\n', pos + sizeof(kHpPrinterLanguageKeyword));
  }
  if (pos == std::string::npos) {
    return false;
  }
  ppd.insert(++pos,
             base::StrCat({"*chromeOSHplipPluginPath: \"", path, "\"\n"}));
  return true;
}

// Configures printers by downloading PPDs then adding them to CUPS through
// debugd.  This class must be used on the UI thread.
class PrinterConfigurerImpl : public PrinterConfigurer {
 public:
  PrinterConfigurerImpl(scoped_refptr<PpdProvider> ppd_provider,
                        DlcserviceClient* dlc_service_client)
      : ppd_provider_(ppd_provider), dlc_service_client_(dlc_service_client) {
    DCHECK(ppd_provider_);
    DCHECK(dlc_service_client_);
  }

  PrinterConfigurerImpl(const PrinterConfigurerImpl&) = delete;
  PrinterConfigurerImpl& operator=(const PrinterConfigurerImpl&) = delete;

  ~PrinterConfigurerImpl() override {}

  void SetUpPrinterInCups(const Printer& printer,
                          PrinterSetupCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!printer.id().empty());
    DCHECK(printer.HasUri());
    PRINTER_LOG(USER) << printer.id() << ": Printer setup requested for "
                      << printer.make_and_model();

    if (!printer.IsIppEverywhere()) {
      if (!printer.ppd_reference().user_supplied_ppd_url.empty()) {
        // The PPD was provided by the user.
        ResolvePpd(printer, /*hplip_plugin_path=*/"", std::move(callback));
      } else {
        // The PPD was selected from our PPD Index. We have to check its license
        // to make sure it doesn't need any additional plugins before setup.
        PRINTER_LOG(DEBUG) << printer.id() << ": Check PPD license";
        ppd_provider_->ResolvePpdLicense(
            printer.ppd_reference().effective_make_and_model,
            base::BindOnce(&PrinterConfigurerImpl::ResolveLicenseDone,
                           weak_factory_.GetWeakPtr(), printer,
                           std::move(callback)));
      }
      return;
    }

    PRINTER_LOG(DEBUG) << printer.id() << ": Attempting driverless setup at "
                       << printer.uri().GetNormalized(
                              /*always_print_port=*/true);
    if (base::FeatureList::IsEnabled(
            printing::features::kAddPrinterViaPrintscanmgr)) {
      printscanmgr::CupsAddAutoConfiguredPrinterRequest request;
      request.set_name(printer.id());
      request.set_uri(printer.uri().GetNormalized(/*always_print_port=*/true));
      request.set_language(g_browser_process->GetApplicationLocale());
      PrintscanmgrClient::Get()->CupsAddAutoConfiguredPrinter(
          std::move(request),
          base::BindOnce(
              &PrinterConfigurerImpl::OnAddedPrinter<
                  printscanmgr::CupsAddAutoConfiguredPrinterResponse>,
              weak_factory_.GetWeakPtr(), printer, std::move(callback)));
    } else {
      DebugDaemonClient::Get()->CupsAddAutoConfiguredPrinter(
          printer.id(), printer.uri().GetNormalized(true /*always_print_port*/),
          g_browser_process->GetApplicationLocale(),
          base::BindOnce(&PrinterConfigurerImpl::OnAddedPrinterDebugd,
                         weak_factory_.GetWeakPtr(), printer,
                         std::move(callback)));
    }
  }

 private:
  // Receive the callback from the printscanmgr daemon client once we attempt to
  // add the printer.
  template <typename T>
  void OnAddedPrinter(const Printer& printer,
                      PrinterSetupCallback cb,
                      std::optional<T> response) {
    // It's expected that the printscanmgr daemon posts callbacks on the UI
    // thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (!response) {
      PRINTER_LOG(ERROR) << printer.id() << ": Null response to OnAddedPrinter";
      std::move(cb).Run(PrinterSetupResult::kFatalError);
      return;
    }

    PrinterSetupResult setup_result =
        PrinterSetupResultFromAddPrinterResult(printer, response->result());
    std::move(cb).Run(setup_result);
  }

  // Receive the callback from the debug daemon client once we attempt to
  // add the printer.
  void OnAddedPrinterDebugd(const Printer& printer,
                            PrinterSetupCallback cb,
                            int32_t result_code) {
    // It's expected that debug daemon posts callbacks on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    PrinterSetupResult setup_result =
        result_code < 0
            ? PrinterSetupResultFromDbusErrorCode(
                  printer, static_cast<chromeos::DBusLibraryError>(result_code))
            : PrinterSetupResultFromDbusResultCode(printer, result_code);
    std::move(cb).Run(setup_result);
  }

  void AddPrinter(const Printer& printer,
                  const std::string& ppd_contents,
                  PrinterSetupCallback cb) {
    PRINTER_LOG(EVENT) << printer.id() << ": Attempting setup with PPD at "
                       << printer.uri().GetNormalized(
                              /*always_print_port=*/true);
    if (base::FeatureList::IsEnabled(
            printing::features::kAddPrinterViaPrintscanmgr)) {
      printscanmgr::CupsAddManuallyConfiguredPrinterRequest request;
      request.set_name(printer.id());
      request.set_uri(printer.uri().GetNormalized(/*always_print_port=*/true));
      request.set_ppd_contents(ppd_contents);
      request.set_language(g_browser_process->GetApplicationLocale());
      PrintscanmgrClient::Get()->CupsAddManuallyConfiguredPrinter(
          std::move(request),
          base::BindOnce(
              &PrinterConfigurerImpl::OnAddedPrinter<
                  printscanmgr::CupsAddManuallyConfiguredPrinterResponse>,
              weak_factory_.GetWeakPtr(), printer, std::move(cb)));
    } else {
      DebugDaemonClient::Get()->CupsAddManuallyConfiguredPrinter(
          printer.id(), printer.uri().GetNormalized(true /*always_print_port*/),
          g_browser_process->GetApplicationLocale(), ppd_contents,
          base::BindOnce(&PrinterConfigurerImpl::OnAddedPrinterDebugd,
                         weak_factory_.GetWeakPtr(), printer, std::move(cb)));
    }
  }

  void ResolvePpdDone(const Printer& printer,
                      const std::string& hplip_plugin_path,
                      PrinterSetupCallback cb,
                      PpdProvider::CallbackResultCode result,
                      const std::string& ppd_contents) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    PRINTER_LOG(EVENT) << printer.id() << " PPD Resolution Result: "
                       << PpdProvider::CallbackResultCodeName(result);
    switch (result) {
      case PpdProvider::SUCCESS:
        DCHECK(!ppd_contents.empty());
        {
          // Use PpdLineReader to ungzip the content (if gzipped).
          auto reader = chromeos::PpdLineReader::Create(ppd_contents);
          std::string ppd = reader->RemainingContent();
          if (reader->Error()) {
            PRINTER_LOG(ERROR)
                << printer.id() << ": Error when reading/decompressing PPD";
          }
          if (!hplip_plugin_path.empty()) {
            if (!AddHplipPluginPathToPpdContent(hplip_plugin_path, ppd)) {
              PRINTER_LOG(ERROR) << printer.id()
                                 << ": Missing HP printer language in PPD file";
            }
          }
          AddPrinter(printer, ppd, std::move(cb));
        }
        break;
      case PpdProvider::CallbackResultCode::NOT_FOUND:
        std::move(cb).Run(PrinterSetupResult::kPpdNotFound);
        break;
      case PpdProvider::CallbackResultCode::SERVER_ERROR:
        std::move(cb).Run(PrinterSetupResult::kPpdUnretrievable);
        break;
      case PpdProvider::CallbackResultCode::INTERNAL_ERROR:
        std::move(cb).Run(PrinterSetupResult::kFatalError);
        break;
      case PpdProvider::CallbackResultCode::PPD_TOO_LARGE:
        std::move(cb).Run(PrinterSetupResult::kPpdTooLarge);
        break;
    }
  }

  void ResolvePpd(const Printer& printer,
                  const std::string& hplip_plugin_path,
                  PrinterSetupCallback cb) {
    PRINTER_LOG(DEBUG) << printer.id() << ": Looking up PPD";
    ppd_provider_->ResolvePpd(
        printer.ppd_reference(),
        base::BindOnce(&PrinterConfigurerImpl::ResolvePpdDone,
                       weak_factory_.GetWeakPtr(), printer, hplip_plugin_path,
                       std::move(cb)));
  }

  void ResolveLicenseDone(const Printer& printer,
                          PrinterSetupCallback cb,
                          PpdProvider::CallbackResultCode result,
                          const std::string& license_name) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    PRINTER_LOG(EVENT) << printer.id() << ": License resolution Result: "
                       << PpdProvider::CallbackResultCodeName(result);
    switch (result) {
      case PpdProvider::SUCCESS:
        break;
      case PpdProvider::CallbackResultCode::NOT_FOUND:
        std::move(cb).Run(PrinterSetupResult::kPpdNotFound);
        return;
      case PpdProvider::CallbackResultCode::SERVER_ERROR:
        std::move(cb).Run(PrinterSetupResult::kPpdUnretrievable);
        return;
      case PpdProvider::CallbackResultCode::INTERNAL_ERROR:
        std::move(cb).Run(PrinterSetupResult::kFatalError);
        return;
      case PpdProvider::CallbackResultCode::PPD_TOO_LARGE:
        std::move(cb).Run(PrinterSetupResult::kPpdTooLarge);
        return;
    }

    if (license_name == kEbuildWithHplipPlugins) {
      // Printers with this license require special plugin. We have to install
      // it before proceeding.
      PRINTER_LOG(DEBUG) << printer.id() << ": Installing hplip-plugin";
      dlcservice::InstallRequest install_request;
      install_request.set_id(kEbuildWithHplipPlugins);
      dlc_service_client_->Install(
          install_request,
          base::BindOnce(&PrinterConfigurerImpl::OnPluginInstallationComplete,
                         weak_factory_.GetWeakPtr(), printer, std::move(cb)),
          base::DoNothing());
    } else {
      // Proceed with PPD resolution.
      ResolvePpd(printer, /*hplip_plugin_path=*/"", std::move(cb));
    }
  }

  void OnPluginInstallationComplete(
      const Printer& printer,
      PrinterSetupCallback cb,
      const DlcserviceClient::InstallResult& result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (result.root_path.empty()) {
      // Empty path of the plugin location means failure.
      PRINTER_LOG(ERROR) << printer.id() << ": Cannot install plugin "
                         << result.dlc_id << ": " << result.error;
      std::move(cb).Run(PrinterSetupResult::kComponentUnavailable);
    } else {
      // Plugin installed. We can proceed with PPD resolution.
      ResolvePpd(printer, result.root_path, std::move(cb));
    }
  }

  scoped_refptr<PpdProvider> ppd_provider_;
  raw_ptr<DlcserviceClient> dlc_service_client_;
  base::WeakPtrFactory<PrinterConfigurerImpl> weak_factory_{this};
};

}  // namespace

// static
std::string PrinterConfigurer::SetupFingerprint(const Printer& printer) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, printer.id());
  base::MD5Update(&ctx, printer.uri().GetNormalized(false));
  base::MD5Update(&ctx, printer.ppd_reference().user_supplied_ppd_url);
  base::MD5Update(&ctx, printer.ppd_reference().effective_make_and_model);
  char autoconf = printer.ppd_reference().autoconf ? 1 : 0;
  base::MD5Update(&ctx, std::string(&autoconf, sizeof(autoconf)));
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return std::string(reinterpret_cast<char*>(&digest.a[0]), sizeof(digest.a));
}

// static
void PrinterConfigurer::RecordUsbPrinterSetupSource(
    UsbPrinterSetupSource source) {
  base::UmaHistogramEnumeration("Printing.CUPS.UsbSetupSource", source);
}

// static
std::unique_ptr<PrinterConfigurer> PrinterConfigurer::Create(
    scoped_refptr<PpdProvider> ppd_provider,
    DlcserviceClient* dlc_service_client) {
  return std::make_unique<PrinterConfigurerImpl>(ppd_provider,
                                                 dlc_service_client);
}

// static
GURL PrinterConfigurer::GeneratePrinterEulaUrl(const std::string& license) {
  GURL eula_url(chrome::kChromeUIOSCreditsURL);
  // Construct the URL with proper reference fragment.
  GURL::Replacements replacements;
  replacements.SetRefStr(license);
  return eula_url.ReplaceComponents(replacements);
}

std::string ResultCodeToMessage(const PrinterSetupResult result) {
  switch (result) {
    // Success.
    case PrinterSetupResult::kSuccess:
      return "Printer successfully configured.";
    case PrinterSetupResult::kEditSuccess:
      return "Printer successfully updated.";
    // Invalid configuration.
    case PrinterSetupResult::kNativePrintersNotAllowed:
      return "Unable to add or edit printer due to enterprise policy.";
    case PrinterSetupResult::kBadUri:
      return "Invalid URI.";
    case PrinterSetupResult::kInvalidPrinterUpdate:
      return "Requested printer changes would make printer unusable.";
    // Problem with a printer.
    case PrinterSetupResult::kPrinterUnreachable:
      return "Could not contact printer for configuration.";
    case PrinterSetupResult::kPrinterSentWrongResponse:
      return "Printer sent unexpected response.";
    case PrinterSetupResult::kPrinterIsNotAutoconfigurable:
      return "Printer is not autoconfigurable.";
    // Problem with a PPD file.
    case PrinterSetupResult::kPpdTooLarge:
      return "PPD is too large.";
    case PrinterSetupResult::kInvalidPpd:
      return "Provided PPD is invalid.";
    case PrinterSetupResult::kPpdNotFound:
      return "Could not locate requested PPD. Check printer configuration.";
    case PrinterSetupResult::kPpdUnretrievable:
      return "Could not retrieve PPD from server. Check Internet connection.";
    // Cannot load a required compomonent.
    case PrinterSetupResult::kComponentUnavailable:
      return "Could not install component.";
    // Problem with D-Bus.
    case PrinterSetupResult::kDbusError:
      return "D-Bus error occurred. Reboot required.";
    case PrinterSetupResult::kDbusNoReply:
      return "Deprecated.";
    case PrinterSetupResult::kDbusTimeout:
      return "Timed out trying to reach printscanmgr over D-Bus.";
    // Problem reported by OS.
    case PrinterSetupResult::kIoError:
      return "I/O error occurred.";
    case PrinterSetupResult::kMemoryAllocationError:
      return "Memory allocation error occurred.";
    // Unknown problem.
    case PrinterSetupResult::kFatalError:
      return "Unknown error occurred.";
    // Printer requires manual setup.
    case PrinterSetupResult::kManualSetupRequired:
      return "Printer requires manual setup.";
    case PrinterSetupResult::kPrinterRemoved:
      return "Printer was removed during the setup.";
    case PrinterSetupResult::kPrintscanmgrDbusNoReply:
      return "Couldn't talk to printscanmgr over D-Bus.";
    case PrinterSetupResult::kDebugdDbusNoReply:
      return "Couldn't talk to debugd over D-Bus.";
  }
}

}  // namespace ash
