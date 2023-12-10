// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_proxy_service_delegate_impl.h"

#include <utility>

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

using ::chromeos::Printer;

CupsProxyServiceDelegateImpl::CupsProxyServiceDelegateImpl(Profile* profile)
    : profile_(profile),
      printers_manager_(
          CupsPrintersManagerFactory::GetForBrowserContext(profile_)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CupsProxyServiceDelegateImpl::~CupsProxyServiceDelegateImpl() = default;

bool CupsProxyServiceDelegateImpl::IsPrinterAccessAllowed() const {
  const PrefService* prefs = profile_->GetPrefs();
  return prefs->GetBoolean(prefs::kPrintingEnabled) &&
         prefs->GetBoolean(plugin_vm::prefs::kPluginVmPrintersAllowed);
}

std::optional<Printer> CupsProxyServiceDelegateImpl::GetPrinter(
    const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return printers_manager_->GetPrinter(id);
}

// TODO(crbug.com/945409): Incorporate printer limit workaround.
std::vector<Printer> CupsProxyServiceDelegateImpl::GetPrinters(
    chromeos::PrinterClass printer_class) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/945409): Include saved + enterprise (+ephemeral?).
  return printers_manager_->GetPrinters(printer_class);
}

std::vector<std::string>
CupsProxyServiceDelegateImpl::GetRecentlyUsedPrinters() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* sticky_settings = printing::PrintPreviewStickySettings::GetInstance();
  CHECK(sticky_settings);
  sticky_settings->RestoreFromPrefs(profile_->GetPrefs());
  return sticky_settings->GetRecentlyUsedPrinters();
}

bool CupsProxyServiceDelegateImpl::IsPrinterInstalled(const Printer& printer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return printers_manager_->IsPrinterInstalled(printer);
}

scoped_refptr<base::SingleThreadTaskRunner>
CupsProxyServiceDelegateImpl::GetIOTaskRunner() {
  return content::GetIOThreadTaskRunner({});
}

void CupsProxyServiceDelegateImpl::SetupPrinter(
    const Printer& printer,
    cups_proxy::SetupPrinterCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Grab current runner to post |cb| to.
  auto cb_runner = base::SequencedTaskRunner::GetCurrentDefault();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CupsProxyServiceDelegateImpl::SetupPrinterOnUIThread,
                     weak_factory_.GetWeakPtr(), printer,
                     base::BindPostTask(std::move(cb_runner), std::move(cb))));
}

// Runs on UI thread.
void CupsProxyServiceDelegateImpl::SetupPrinterOnUIThread(
    const Printer& printer,
    cups_proxy::SetupPrinterCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  printers_manager_->SetUpPrinter(
      printer, /*is_automatic_installation=*/true,
      base::BindOnce(&CupsProxyServiceDelegateImpl::OnSetupPrinter,
                     weak_factory_.GetWeakPtr(), std::move(cb)));
}

void CupsProxyServiceDelegateImpl::OnSetupPrinter(
    cups_proxy::SetupPrinterCallback cb,
    PrinterSetupResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(cb).Run(result == PrinterSetupResult::kSuccess);
}

}  // namespace ash
