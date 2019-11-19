// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_configurer.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

const std::map<const std::string, const std::string>&
GetComponentizedFilters() {
  // A mapping from filter names to available components for downloads.
  static const auto* const componentized_filters =
      new std::map<const std::string, const std::string>{
          {"epson-escpr-wrapper", "epson-inkjet-printer-escpr"},
          {"epson-escpr", "epson-inkjet-printer-escpr"},
          {"rastertostar", "star-cups-driver"},
          {"rastertostarlm", "star-cups-driver"}};
  return *componentized_filters;
}

namespace chromeos {

namespace {

PrinterSetupResult PrinterSetupResultFromDbusResultCode(const Printer& printer,
                                                        int result_code) {
  DCHECK_GE(result_code, 0);
  switch (result_code) {
    case debugd::CupsResult::CUPS_SUCCESS:
      PRINTER_LOG(DEBUG) << printer.make_and_model()
                         << " Printer setup successful";
      return PrinterSetupResult::kSuccess;
    case debugd::CupsResult::CUPS_INVALID_PPD:
      PRINTER_LOG(EVENT) << printer.make_and_model() << " PPD Invalid";
      return PrinterSetupResult::kInvalidPpd;
    case debugd::CupsResult::CUPS_AUTOCONF_FAILURE:
      PRINTER_LOG(EVENT) << printer.make_and_model() << " Autoconf failed";
      // There are other reasons autoconf fails but this is the most likely.
      return PrinterSetupResult::kPrinterUnreachable;
    case debugd::CupsResult::CUPS_LPADMIN_FAILURE:
      // Printers should always be configurable by lpadmin.
      PRINTER_LOG(ERROR) << printer.make_and_model()
                         << " lpadmin could not add the printer";
      return PrinterSetupResult::kFatalError;
    case debugd::CupsResult::CUPS_FATAL:
    default:
      // We have no idea.  It must be fatal.
      PRINTER_LOG(ERROR) << printer.make_and_model()
                         << " Unrecognized printer setup error: "
                         << result_code;
      return PrinterSetupResult::kFatalError;
  }
}

// Map D-Bus errors from the debug daemon client to D-Bus errors enumerated
// in PrinterSetupResult.
PrinterSetupResult PrinterSetupResultFromDbusErrorCode(
    DbusLibraryError dbus_error) {
  DCHECK_LT(dbus_error, 0);
  static const base::NoDestructor<
      base::flat_map<DbusLibraryError, PrinterSetupResult>>
      kDbusErrorMap({
          {DbusLibraryError::kNoReply, PrinterSetupResult::kDbusNoReply},
          {DbusLibraryError::kTimeout, PrinterSetupResult::kDbusTimeout},
      });
  auto it = kDbusErrorMap->find(dbus_error);
  return it != kDbusErrorMap->end() ? it->second
                                    : PrinterSetupResult::kDbusError;
}

// Records whether a |printer| contains a valid PpdReference defined as having
// either autoconf or a ppd reference set.
void RecordValidPpdReference(const Printer& printer) {
  const auto& ppd_ref = printer.ppd_reference();
  // A PpdReference is valid if exactly one field is set in PpdReference.
  int refs = ppd_ref.autoconf ? 1 : 0;
  refs += !ppd_ref.user_supplied_ppd_url.empty() ? 1 : 0;
  refs += !ppd_ref.effective_make_and_model.empty() ? 1 : 0;
  base::UmaHistogramBoolean("Printing.CUPS.ValidPpdReference", refs == 1);
}

// Configures printers by downloading PPDs then adding them to CUPS through
// debugd.  This class must be used on the UI thread.
class PrinterConfigurerImpl : public PrinterConfigurer {
 public:
  explicit PrinterConfigurerImpl(Profile* profile)
      : ppd_provider_(CreatePpdProvider(profile)) {}

  ~PrinterConfigurerImpl() override {}

  void SetUpPrinter(const Printer& printer,
                    PrinterSetupCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!printer.id().empty());
    DCHECK(!printer.uri().empty());
    PRINTER_LOG(USER) << printer.make_and_model() << " Printer setup requested";
    // Record if autoconf and a PPD are set.  crbug.com/814374.
    RecordValidPpdReference(printer);

    if (!printer.IsIppEverywhere()) {
      PRINTER_LOG(DEBUG) << printer.make_and_model() << " Lookup PPD";
      ppd_provider_->ResolvePpd(
          printer.ppd_reference(),
          base::BindOnce(&PrinterConfigurerImpl::ResolvePpdDone,
                         weak_factory_.GetWeakPtr(), printer,
                         std::move(callback)));
      return;
    }

    PRINTER_LOG(DEBUG) << printer.make_and_model()
                       << " Attempting autoconf setup";
    auto* client = DBusThreadManager::Get()->GetDebugDaemonClient();
    client->CupsAddAutoConfiguredPrinter(
        printer.id(), printer.uri(),
        base::BindOnce(&PrinterConfigurerImpl::OnAddedPrinter,
                       weak_factory_.GetWeakPtr(), printer,
                       std::move(callback)));
  }

 private:
  // Receive the callback from the debug daemon client once we attempt to
  // add the printer.
  void OnAddedPrinter(const Printer& printer,
                      PrinterSetupCallback cb,
                      int32_t result_code) {
    // It's expected that debug daemon posts callbacks on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    PrinterSetupResult setup_result =
        result_code < 0
            ? PrinterSetupResultFromDbusErrorCode(
                  static_cast<DbusLibraryError>(result_code))
            : PrinterSetupResultFromDbusResultCode(printer, result_code);
    std::move(cb).Run(setup_result);
  }

  void AddPrinter(const Printer& printer,
                  const std::string& ppd_contents,
                  PrinterSetupCallback cb) {
    auto* client = DBusThreadManager::Get()->GetDebugDaemonClient();

    PRINTER_LOG(EVENT) << printer.make_and_model() << " Manual printer setup";
    client->CupsAddManuallyConfiguredPrinter(
        printer.id(), printer.uri(), ppd_contents,
        base::BindOnce(&PrinterConfigurerImpl::OnAddedPrinter,
                       weak_factory_.GetWeakPtr(), printer, std::move(cb)));
  }

  // Executed on component load API finish.
  // Check API return result to decide whether component is successfully loaded.
  void OnComponentLoad(const Printer& printer,
                       const std::string& ppd_contents,
                       PrinterSetupCallback cb,
                       component_updater::CrOSComponentManager::Error error,
                       const base::FilePath& path) {
    if (error != component_updater::CrOSComponentManager::Error::NONE) {
      PRINTER_LOG(ERROR) << printer.make_and_model()
                         << " Filter component installation fails.";
      std::move(cb).Run(PrinterSetupResult::kComponentUnavailable);
    } else {
      AddPrinter(printer, ppd_contents, std::move(cb));
    }
  }

  void ResolvePpdSuccess(const Printer& printer,
                         PrinterSetupCallback cb,
                         const std::string& ppd_contents,
                         const std::vector<std::string>& ppd_filters) {
    std::set<std::string> components_requested;
    for (const auto& ppd_filter : ppd_filters) {
      for (const auto& component : GetComponentizedFilters()) {
        if (component.first == ppd_filter) {
          components_requested.insert(component.second);
        }
      }
    }
    if (components_requested.size() == 1) {
      // Only allow one filter request in ppd file.
      auto& component_name = *components_requested.begin();
      g_browser_process->platform_part()->cros_component_manager()->Load(
          component_name,
          component_updater::CrOSComponentManager::MountPolicy::kMount,
          component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
          base::BindOnce(&PrinterConfigurerImpl::OnComponentLoad,
                         weak_factory_.GetWeakPtr(), printer, ppd_contents,
                         std::move(cb)));
      return;
    }
    if (components_requested.size() > 1) {
      PRINTER_LOG(ERROR) << printer.make_and_model()
                         << " More than one filter component is requested.";
      std::move(cb).Run(PrinterSetupResult::kFatalError);
      return;
    }
    AddPrinter(printer, ppd_contents, std::move(cb));
  }

  void ResolvePpdDone(const Printer& printer,
                      PrinterSetupCallback cb,
                      PpdProvider::CallbackResultCode result,
                      const std::string& ppd_contents,
                      const std::vector<std::string>& ppd_filters) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    PRINTER_LOG(EVENT) << printer.make_and_model()
                       << " PPD Resolution Result: " << result;
    switch (result) {
      case PpdProvider::SUCCESS:
        DCHECK(!ppd_contents.empty());
        ResolvePpdSuccess(printer, std::move(cb), ppd_contents, ppd_filters);
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

  scoped_refptr<PpdProvider> ppd_provider_;
  base::WeakPtrFactory<PrinterConfigurerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrinterConfigurerImpl);
};

}  // namespace

// static
std::string PrinterConfigurer::SetupFingerprint(const Printer& printer) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, printer.id());
  base::MD5Update(&ctx, printer.uri());
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
std::unique_ptr<PrinterConfigurer> PrinterConfigurer::Create(Profile* profile) {
  return std::make_unique<PrinterConfigurerImpl>(profile);
}

std::ostream& operator<<(std::ostream& out, const PrinterSetupResult& result) {
  switch (result) {
    case kFatalError:
      out << "fatal error";
      break;
    case kSuccess:
      out << "add success";
      break;
    case kEditSuccess:
      out << "edit success";
      break;
    case kPrinterUnreachable:
      out << "printer unreachable";
      break;
    case kDbusError:
      out << "failed to connect over dbus";
      break;
    case kNativePrintersNotAllowed:
      out << "native printers denied by policy";
      break;
    case kInvalidPrinterUpdate:
      out << "printer edits would make printer unusable";
      break;
    case kComponentUnavailable:
      out << "component driver was requested but installation failed.";
      break;
    case kPpdTooLarge:
      out << "PPD too large";
      break;
    case kInvalidPpd:
      out << "PPD rejected by cupstestppd";
      break;
    case kPpdNotFound:
      out << "could not find PPD";
      break;
    case kPpdUnretrievable:
      out << "failed to download PPD";
      break;
    case kDbusNoReply:
      out << "no reply from debugd";
      break;
    case kDbusTimeout:
      out << "timeout in D-Bus";
      break;
    case kMaxValue:
      out << "unexpected result";
      break;
  }

  return out;
}

}  // namespace chromeos
