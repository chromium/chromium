// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_proxy_service_delegate_impl.h"

#include <utility>

#include "base/task/post_task.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"

namespace chromeos {

// TODO(crbug.com/945409): Decide on correct profile/s to use.
CupsProxyServiceDelegateImpl::CupsProxyServiceDelegateImpl()
    : profile_(ProfileManager::GetPrimaryUserProfile()),
      printers_manager_(
          CupsPrintersManagerFactory::GetForBrowserContext(profile_)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CupsProxyServiceDelegateImpl::~CupsProxyServiceDelegateImpl() = default;

base::Optional<Printer> CupsProxyServiceDelegateImpl::GetPrinter(
    const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return printers_manager_->GetPrinter(id);
}

// TODO(crbug.com/945409): Incorporate printer limit workaround.
std::vector<Printer> CupsProxyServiceDelegateImpl::GetPrinters(
    PrinterClass printer_class) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/945409): Include saved + enterprise (+ephemeral?).
  return printers_manager_->GetPrinters(printer_class);
}

bool CupsProxyServiceDelegateImpl::IsPrinterInstalled(const Printer& printer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return printers_manager_->IsPrinterInstalled(printer);
}

// Expects |printer| is known by the printers_manager_.
void CupsProxyServiceDelegateImpl::PrinterInstalled(const Printer& printer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetPrinter(printer.id()));
  printers_manager_->PrinterInstalled(
      printer, false /* unused */, PrinterSetupSource::kMaxValue /* unused */);
}

scoped_refptr<base::SingleThreadTaskRunner>
CupsProxyServiceDelegateImpl::GetIOTaskRunner() {
  return base::CreateSingleThreadTaskRunner({content::BrowserThread::IO});
}

void CupsProxyServiceDelegateImpl::SetupPrinter(
    const Printer& printer,
    cups_proxy::SetupPrinterCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Grab current runner to post |cb| to.
  auto cb_runner = base::SequencedTaskRunnerHandle::Get();
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&CupsProxyServiceDelegateImpl::SetupPrinterOnThread,
                     weak_factory_.GetWeakPtr(), printer,
                     base::Passed(&cb_runner), std::move(cb)));
}

// Runs on UI thread.
void CupsProxyServiceDelegateImpl::SetupPrinterOnThread(
    const Printer& printer,
    scoped_refptr<base::SequencedTaskRunner> cb_runner,
    cups_proxy::SetupPrinterCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Lazily grab the configurer while on the UI thread.
  if (!printer_configurer_) {
    printer_configurer_ = PrinterConfigurer::Create(profile_);
  }

  printer_configurer_->SetUpPrinter(
      printer, base::BindOnce(&CupsProxyServiceDelegateImpl::OnSetupPrinter,
                              weak_factory_.GetWeakPtr(),
                              base::Passed(&cb_runner), std::move(cb)));
}

// |printer_configurer| unused but ensures this callback outlives it.
void CupsProxyServiceDelegateImpl::OnSetupPrinter(
    scoped_refptr<base::SequencedTaskRunner> cb_runner,
    cups_proxy::SetupPrinterCallback cb,
    PrinterSetupResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  cb_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(cb), result == PrinterSetupResult::kSuccess));
}

}  // namespace chromeos
