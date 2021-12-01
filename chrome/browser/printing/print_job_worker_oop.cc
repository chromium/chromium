// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_worker_oop.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/printed_document.h"
#include "printing/printing_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using content::BrowserThread;

namespace printing {

namespace {

mojom::PrintTargetType DeterminePrintTargetType(
    const base::Value& job_settings) {
#if defined(OS_MAC)
  if (job_settings.FindKey(kSettingOpenPDFInPreview))
    return mojom::PrintTargetType::kExternalPreview;
#endif
  if (job_settings.FindBoolKey(kSettingShowSystemDialog).value_or(false))
    return mojom::PrintTargetType::kSystemDialog;
  return mojom::PrintTargetType::kDirectToDevice;
}

}  // namespace

PrintJobWorkerOop::PrintJobWorkerOop(int render_process_id, int render_frame_id)
    : PrintJobWorker(render_process_id, render_frame_id) {}

PrintJobWorkerOop::~PrintJobWorkerOop() {
  DCHECK(!service_manager_client_id_.has_value());
}

void PrintJobWorkerOop::StartPrinting(PrintedDocument* new_document) {
  if (!StartPrintingSanityCheck(new_document))
    return;

  // Do browser-side context setup.
  std::u16string document_name = GetDocumentName(new_document);
  mojom::ResultCode result = printing_context()->NewDocument(document_name);
  if (result != mojom::ResultCode::kSuccess) {
    OnFailure();
    return;
  }

  std::string device_name =
      base::UTF16ToUTF8(document()->settings().device_name());
  VLOG(1) << "Start printing document " << document()->cookie() << " to "
          << device_name;

  // `PrintBackendServiceManager` interactions must happen on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJobWorkerOop::SendStartPrinting,
                                ui_weak_factory_.GetWeakPtr(), device_name,
                                document_name));
}

void PrintJobWorkerOop::OnDidStartPrinting(mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR) << "Error initiating printing via service for document "
                       << document()->cookie() << ": " << result;
    if (result == mojom::ResultCode::kAccessDenied) {
      // Register that this printer requires elevated privileges.
      PrintBackendServiceManager& service_mgr =
          PrintBackendServiceManager::GetInstance();
      service_mgr.SetPrinterDriverRequiresElevatedPrivilege(device_name_);

      // Failure from access-denied means we no longer need this client.
      UnregisterServiceManagerClient();

      // Retry the operation which should now happen at a higher privilege
      // level.
      SendStartPrinting(device_name_, document_name_);
      return;
    }
    task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(&PrintJobWorkerOop::OnFailure,
                                           worker_weak_factory_.GetWeakPtr()));
    return;
  }
  VLOG(1) << "Printing initiated with service for document "
          << document()->cookie();
  // TODO(crbug.com/809738)  Still need more support for printing pipeline in
  // the service.
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&PrintJobWorkerOop::OnFailure,
                                         worker_weak_factory_.GetWeakPtr()));
}

void PrintJobWorkerOop::UpdatePrintSettings(base::Value new_settings,
                                            SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't use as a const reference, since that reference into `new_settings`
  // isn't safe after TakeDict() destroys the internal dictionary for it.
  std::string device_name = *new_settings.FindStringKey(kSettingDeviceName);

  // Save the print target type from the settings, since this will be needed
  // later when printing is started.
  print_target_type_ = DeterminePrintTargetType(new_settings);

  VLOG(1) << "Updating print settings via service for " << device_name;
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.UpdatePrintSettings(
      device_name, std::move(new_settings).TakeDict(),
      base::BindOnce(&PrintJobWorkerOop::OnDidUpdatePrintSettings,
                     ui_weak_factory_.GetWeakPtr(), device_name,
                     std::move(callback)));
}

void PrintJobWorkerOop::OnFailure() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintJobWorkerOop::UnregisterServiceManagerClient,
                     ui_weak_factory_.GetWeakPtr()));
  PrintJobWorker::OnFailure();
}

void PrintJobWorkerOop::UnregisterServiceManagerClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_manager_client_id_.has_value()) {
    PrintBackendServiceManager::GetInstance().UnregisterClient(
        service_manager_client_id_.value());
    service_manager_client_id_.reset();
  }
}

void PrintJobWorkerOop::OnDidUpdatePrintSettings(
    const std::string& device_name,
    SettingsCallback callback,
    mojom::PrintSettingsResultPtr print_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::ResultCode result;
  if (print_settings->is_result_code()) {
    result = print_settings->get_result_code();
    DCHECK_NE(result, mojom::ResultCode::kSuccess);
    PRINTER_LOG(ERROR) << "Failure to update print settings for " << device_name
                       << " - error " << result;

    // TODO(crbug.com/809738)  Fill in support for handling of access-denied
    // result code.
  } else {
    VLOG(1) << "Update print settings from service complete for "
            << device_name;
    result = mojom::ResultCode::kSuccess;
    printing_context()->ApplyPrintSettings(print_settings->get_settings());
  }
  GetSettingsDone(std::move(callback), result);
}

void PrintJobWorkerOop::SendStartPrinting(const std::string& device_name,
                                          const std::u16string& document_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(features::kEnableOopPrintDriversJobPrint.Get());

  // The device name is needed repeatedly for each call to the service, cache
  // that for this print job.
  device_name_ = device_name;

  // Save the document name in case it is needed for retrying a job after
  // failure.
  document_name_ = document_name;

  const int32_t document_cookie = document()->cookie();
  VLOG(1) << "Starting printing via service for to `" << device_name_
          << "` for document " << document_cookie;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  // Register this worker as a printing client.
  service_manager_client_id_ = service_mgr.RegisterClient();

  service_mgr.StartPrinting(
      device_name_, document_cookie, document_name_, print_target_type_,
      document()->settings(),
      base::BindOnce(&PrintJobWorkerOop::OnDidStartPrinting,
                     ui_weak_factory_.GetWeakPtr()));
}

}  // namespace printing
