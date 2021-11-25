// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_worker_oop.h"

#include "base/values.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace printing {

PrintJobWorkerOop::PrintJobWorkerOop(int render_process_id, int render_frame_id)
    : PrintJobWorker(render_process_id, render_frame_id) {}

PrintJobWorkerOop::~PrintJobWorkerOop() = default;

void PrintJobWorkerOop::OnDidStartPrinting(mojom::ResultCode result) {
  // TODO(crbug.com/809738)  Temporary placeholder for testing preparation.
}

void PrintJobWorkerOop::UpdatePrintSettings(base::Value new_settings,
                                            SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't use as a const reference, since that reference into `new_settings`
  // isn't safe after TakeDict() destroys the internal dictionary for it.
  std::string device_name = *new_settings.FindStringKey(kSettingDeviceName);

  VLOG(1) << "Updating print settings via service for " << device_name;
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  // Safe to use base::Unretained(this) since the callback owns `this`, and
  // `service_mgr` is a global instance which never exits and simply wraps
  // `callback` so that it is still called should the service terminate
  // unexpectedly.
  service_mgr.UpdatePrintSettings(
      device_name, std::move(new_settings).TakeDict(),
      base::BindOnce(&PrintJobWorkerOop::OnDidUpdatePrintSettings,
                     base::Unretained(this), device_name, std::move(callback)));
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

}  // namespace printing
