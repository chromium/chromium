// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_query_oop.h"

#include <memory>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_job_worker_oop.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "printing/printing_features.h"

namespace printing {

namespace {

mojom::PrintTargetType DeterminePrintTargetType(
    const base::Value::Dict& job_settings) {
#if BUILDFLAG(IS_MAC)
  if (job_settings.contains(kSettingOpenPDFInPreview)) {
    return mojom::PrintTargetType::kExternalPreview;
  }
#endif
  if (job_settings.FindBool(kSettingShowSystemDialog).value_or(false)) {
    return mojom::PrintTargetType::kSystemDialog;
  }
  return mojom::PrintTargetType::kDirectToDevice;
}

}  // namespace

PrinterQueryOop::PrinterQueryOop(content::GlobalRenderFrameHostId rfh_id)
    : PrinterQuery(rfh_id) {}

PrinterQueryOop::~PrinterQueryOop() = default;

std::unique_ptr<PrintJobWorker> PrinterQueryOop::TransferContextToNewWorker(
    PrintJob* print_job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/1414968)  Do extra setup on the worker as needed for
  // supporting OOP system print dialogs.
  return CreatePrintJobWorker(print_job);
}

void PrinterQueryOop::OnDidUseDefaultSettings(
    SettingsCallback callback,
    mojom::PrintSettingsResultPtr print_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::ResultCode result;
  if (print_settings->is_result_code()) {
    result = print_settings->get_result_code();
    DCHECK_NE(result, mojom::ResultCode::kSuccess);
    PRINTER_LOG(ERROR) << "Error trying to use default settings via service: "
                       << result;

    // TODO(crbug.com/809738)  Fill in support for handling of access-denied
    // result code.  Blocked on crbug.com/1243873 for Windows.
  } else {
    VLOG(1) << "Use default settings from service complete";
    result = mojom::ResultCode::kSuccess;
    printing_context()->ApplyPrintSettings(print_settings->get_settings());
  }

  InvokeSettingsCallback(std::move(callback), result);
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrinterQueryOop::OnDidAskUserForSettings(
    SettingsCallback callback,
    mojom::PrintSettingsResultPtr print_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::ResultCode result;
  if (print_settings->is_result_code()) {
    result = print_settings->get_result_code();
    DCHECK_NE(result, mojom::ResultCode::kSuccess);
    if (result != mojom::ResultCode::kCanceled) {
      PRINTER_LOG(ERROR) << "Error getting settings from user via service: "
                         << result;
    }

    // TODO(crbug.com/809738)  Fill in support for handling of access-denied
    // result code.  Blocked on crbug.com/1243873 for Windows.
  } else {
    VLOG(1) << "Ask user for settings from service complete";
    result = mojom::ResultCode::kSuccess;
    printing_context()->ApplyPrintSettings(print_settings->get_settings());
  }

  InvokeSettingsCallback(std::move(callback), result);
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

void PrinterQueryOop::UseDefaultSettings(SettingsCallback callback) {
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  SendUseDefaultSettings(std::move(callback));
#else
  // `PrintingContextLinux::UseDefaultSettings()` is to be called prior to
  // `AskUserForSettings()` to establish a base device context.  If the system
  // print dialog will be invoked from within the browser process, then that
  // default setup needs to happen in browser as well.
  PrinterQuery::UseDefaultSettings(std::move(callback));
#endif
}

void PrinterQueryOop::GetSettingsWithUI(uint32_t document_page_count,
                                        bool has_selection,
                                        bool is_scripted,
                                        SettingsCallback callback) {
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  SendAskUserForSettings(document_page_count, has_selection, is_scripted,
                         std::move(callback));
#else
  // Invoke the browser version of getting settings with the system UI:
  //   - macOS:  It is impossible to invoke a system dialog UI from a service
  //       utility and have that dialog be application modal for a window that
  //       was launched by the browser process.
  //   - Linux:  TODO(crbug.com/809738)  Determine if Linux Wayland can be made
  //       to have a system dialog be modal against an application window in the
  //       browser process.
  //   - Other platforms don't have a system print UI or do not use OOP
  //     printing, so this does not matter.
  PrinterQuery::GetSettingsWithUI(document_page_count, has_selection,
                                  is_scripted, std::move(callback));
#endif
}

void PrinterQueryOop::UpdatePrintSettings(base::Value::Dict new_settings,
                                          SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Do not take a const reference, as `new_settings` will be modified below.
  std::string device_name = *new_settings.FindString(kSettingDeviceName);

  // Save the print target type from the settings, since this will be needed
  // later when printing is started.
  print_target_type_ = DeterminePrintTargetType(new_settings);

  VLOG(1) << "Updating print settings via service for " << device_name;
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.UpdatePrintSettings(
      device_name, std::move(new_settings),
      base::BindOnce(&PrinterQueryOop::OnDidUpdatePrintSettings,
                     weak_factory_.GetWeakPtr(), device_name,
                     std::move(callback)));
}

void PrinterQueryOop::OnDidUpdatePrintSettings(
    const std::string& device_name,
    SettingsCallback callback,
    mojom::PrintSettingsResultPtr print_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::ResultCode result;
  if (print_settings->is_result_code()) {
    result = print_settings->get_result_code();
    DCHECK_NE(result, mojom::ResultCode::kSuccess);
    PRINTER_LOG(ERROR) << "Error updating print settings via service for `"
                       << device_name << "`: " << result;

    // TODO(crbug.com/809738)  Fill in support for handling of access-denied
    // result code.
  } else {
    VLOG(1) << "Update print settings via service complete for " << device_name;
    result = mojom::ResultCode::kSuccess;
    printing_context()->ApplyPrintSettings(print_settings->get_settings());
  }
  InvokeSettingsCallback(std::move(callback), result);
}

void PrinterQueryOop::SendUseDefaultSettings(SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(features::kEnableOopPrintDriversJobPrint.Get());

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.UseDefaultSettings(
      /*printer_name=*/std::string(),
      base::BindOnce(&PrinterQueryOop::OnDidUseDefaultSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrinterQueryOop::SendAskUserForSettings(uint32_t document_page_count,
                                             bool has_selection,
                                             bool is_scripted,
                                             SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(features::kEnableOopPrintDriversJobPrint.Get());

  if (document_page_count > kMaxPageCount) {
    InvokeSettingsCallback(std::move(callback), mojom::ResultCode::kFailed);
    return;
  }

  // Save the print target type from the settings, since this will be needed
  // later when printing is started.
  print_target_type_ = mojom::PrintTargetType::kDirectToDevice;

  content::WebContents* web_contents = GetWebContents();

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  if (web_contents && web_contents->IsFullscreen()) {
    web_contents->ExitFullscreen(true);
  }

  gfx::NativeView parent_view =
      web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();
  service_mgr.AskUserForSettings(
      /*printer_name=*/std::string(), parent_view, document_page_count,
      has_selection, is_scripted,
      base::BindOnce(&PrinterQueryOop::OnDidAskUserForSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

std::unique_ptr<PrintJobWorkerOop> PrinterQueryOop::CreatePrintJobWorker(
    PrintJob* print_job) {
  return std::make_unique<PrintJobWorkerOop>(
      std::move(printing_context_delegate_), std::move(printing_context_),
      print_job, print_target_type_);
}

}  // namespace printing
