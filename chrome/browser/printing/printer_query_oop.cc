// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_query_oop.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_job_worker_oop.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "printing/buildflags/buildflags.h"

namespace printing {

PrinterQueryOop::PrinterQueryOop(content::GlobalRenderFrameHostId rfh_id)
    : PrinterQuery(rfh_id) {}

PrinterQueryOop::~PrinterQueryOop() = default;

std::unique_ptr<PrintJobWorker> PrinterQueryOop::TransferContextToNewWorker(
    PrintJob* print_job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40256381)  Do extra setup on the worker as needed for
  // supporting OOP system print dialogs.
  return CreatePrintJobWorkerOop(print_job);
}

#if BUILDFLAG(IS_WIN)
void PrinterQueryOop::UpdatePrintableArea(
    PrintSettings* print_settings,
    OnDidUpdatePrintableAreaCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string printer_name = base::UTF16ToUTF8(print_settings->device_name());
  PRINTER_LOG(EVENT) << "Updating paper printable area via service for "
                     << printer_name;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  // Caller is required to ensure `print_settings` stays alive until `callback`
  // runs.
  service_mgr.GetPaperPrintableArea(
      printer_name, print_settings->requested_media(),
      base::BindOnce(&PrinterQueryOop::OnDidGetPaperPrintableArea,
                     weak_factory_.GetWeakPtr(), print_settings,
                     std::move(callback)));
}
#endif

void PrinterQueryOop::SetClientId(
    PrintBackendServiceManager::ClientId client_id) {
  query_with_ui_client_id_ = client_id;
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

    // TODO(crbug.com/40561724)  Fill in support for handling of access-denied
    // result code.  Blocked on crbug.com/1243873 for Windows.
  } else {
    VLOG(1) << "Use default settings from service complete";
    result = mojom::ResultCode::kSuccess;
    printing_context()->SetPrintSettings(print_settings->get_settings());
  }

  InvokeSettingsCallback(std::move(callback), result);
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrinterQueryOop::OnDidAskUserForSettings(
    SettingsCallback callback,
    mojom::PrintSettingsResultPtr print_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::ResultCode result;
  if (print_settings->is_settings()) {
    VLOG(1) << "Ask user for settings from service complete";
    result = mojom::ResultCode::kSuccess;
    printing_context()->SetPrintSettings(print_settings->get_settings());

    // Use the same PrintBackendService for querying and printing, so that the
    // same device context can be used with both.
    print_document_client_id_ =
        PrintBackendServiceManager::GetInstance()
            .RegisterPrintDocumentClientReusingClientRemote(
                *query_with_ui_client_id_);
    if (!print_document_client_id_.has_value()) {
      // A failure after getting settings, override result to failure.
      result = mojom::ResultCode::kFailed;
      PRINTER_LOG(ERROR) << "Error after getting settings from user via "
                            "service due to service unavailable";
    }
  } else {
    result = print_settings->get_result_code();
    DCHECK_NE(result, mojom::ResultCode::kSuccess);
    if (result != mojom::ResultCode::kCanceled) {
      PRINTER_LOG(ERROR) << "Error getting settings from user via service: "
                         << result;
    }

    // TODO(crbug.com/40561724)  Fill in support for handling of access-denied
    // result code.  Blocked on crbug.com/1243873 for Windows.
  }

  InvokeSettingsCallback(std::move(callback), result);
}
#else   // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrinterQueryOop::OnDidAskUserForSettings(
    SettingsCallback callback,
    std::unique_ptr<PrintSettings> new_settings,
    mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result == mojom::ResultCode::kSuccess) {
    // Want the same PrintBackend service as the query so that we use the same
    // device context.
    print_document_client_id_ =
        PrintBackendServiceManager::GetInstance()
            .RegisterPrintDocumentClientReusingClientRemote(
                *query_with_ui_client_id_);
    if (!print_document_client_id_.has_value()) {
      // A failure after getting settings, override result to failure.
      result = mojom::ResultCode::kFailed;
      PRINTER_LOG(ERROR) << "Error after getting settings from user via "
                            "service due to service unavailable";
    }
  }
  std::move(callback).Run(std::move(new_settings), result);
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

void PrinterQueryOop::UseDefaultSettings(SettingsCallback callback) {
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  CHECK(query_with_ui_client_id_.has_value());

  PRINTER_LOG(EVENT) << "Using printer default settings via service";

  // Any settings selected from the system dialog could need to be retained
  // for printing, so establish a printing context.
  CHECK(!context_id_.has_value());
  SendEstablishPrintingContext(*query_with_ui_client_id_,
                               /*printer_name=*/std::string());
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
  // Save the print target type from the settings, since this will be needed
  // later when printing is started.
  print_from_system_dialog_ = true;

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  PRINTER_LOG(EVENT) << "Getting printer settings from user via service";
  SendAskUserForSettings(document_page_count, has_selection, is_scripted,
                         std::move(callback));
#else
  // Invoke the browser version of getting settings with the system UI:
  //   - macOS:  It is impossible to invoke a system dialog UI from a service
  //       utility and have that dialog be application modal for a window that
  //       was launched by the browser process.
  //   - Linux:  TODO(crbug.com/40561724)  Determine if Linux Wayland can be
  //   made
  //       to have a system dialog be modal against an application window in the
  //       browser process.
  //   - Other platforms don't have a system print UI or do not use OOP
  //     printing, so this does not matter.
  PrinterQuery::GetSettingsWithUI(
      document_page_count, has_selection, is_scripted,
      base::BindOnce(&PrinterQueryOop::OnDidAskUserForSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
#endif
}

void PrinterQueryOop::UpdatePrintSettings(base::Value::Dict new_settings,
                                          SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Do not take a const reference, as `new_settings` will be modified below.
  std::string device_name = *new_settings.FindString(kSettingDeviceName);

  // Remember if this is from system print dialog, since this will be needed
  // later when printing is started.
  print_from_system_dialog_ =
      new_settings.FindBool(kSettingShowSystemDialog).value_or(false);

  // A device name is required for printing documents. If the device name is
  // empty then this is for a system print dialog, for which a destination is
  // not yet known.
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();
  PrintBackendServiceManager::ClientId client_id;
  std::string printer_name;
  if (print_from_system_dialog_) {
    CHECK(!print_document_client_id_.has_value());
    client_id = *query_with_ui_client_id_;
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG) && BUILDFLAG(IS_WIN)
    // `PrintingContextWin::UpdatePrintSettings()` is special because it can
    // invoke `AskUserForSettings()` and cause a system dialog to be displayed.
    // Running a dialog causes an exit to webpage-initiated fullscreen.
    // http://crbug.com/728276
    content::WebContents* web_contents = GetWebContents();
    if (web_contents && web_contents->IsFullscreen()) {
      web_contents->ExitFullscreen(true);
    }
#endif
  } else {
    // Print the document from Print Preview.
    CHECK(!query_with_ui_client_id_.has_value());
    CHECK(!print_document_client_id_.has_value());

    print_document_client_id_ =
        service_mgr.RegisterPrintDocumentClient(device_name);
    client_id = *print_document_client_id_;
    printer_name = device_name;
  }
  SendEstablishPrintingContext(client_id, printer_name);

  VLOG(1) << "Updating print settings via service for " << device_name;

  service_mgr.UpdatePrintSettings(
      client_id, printer_name, *context_id_, std::move(new_settings),
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

    // `PrintViewManagerBase` owns the client ID, so `PrinterQueryOop` must not
    // unregister it.  Just drop any local reference to it.
    query_with_ui_client_id_.reset();

    // With the failure to update the setting, the registered client must be
    // released.  The context ID is also no longer relevant to use.
    if (print_document_client_id_.has_value()) {
      PrintBackendServiceManager::GetInstance().UnregisterClient(
          print_document_client_id_.value());
      print_document_client_id_.reset();
      CHECK(context_id_.has_value());
      context_id_.reset();
    }

    // TODO(crbug.com/40561724)  Fill in support for handling of access-denied
    // result code.
  } else {
    VLOG(1) << "Update print settings via service complete for " << device_name;
    result = mojom::ResultCode::kSuccess;
    printing_context()->SetPrintSettings(print_settings->get_settings());

    if (query_with_ui_client_id_.has_value()) {
      // Use the same PrintBackendService for querying and printing, so that the
      // same device context can be used with both.
      CHECK(!print_document_client_id_.has_value());
      print_document_client_id_ =
          PrintBackendServiceManager::GetInstance()
              .RegisterPrintDocumentClientReusingClientRemote(
                  *query_with_ui_client_id_);
      if (!print_document_client_id_.has_value()) {
        // A failure after getting settings, override result to failure.
        result = mojom::ResultCode::kFailed;
        PRINTER_LOG(ERROR) << "Error after updating print settings via service "
                              "due to service unavailable";
      }
    }
  }
  InvokeSettingsCallback(std::move(callback), result);
}

#if BUILDFLAG(IS_WIN)
void PrinterQueryOop::OnDidGetPaperPrintableArea(
    PrintSettings* print_settings,
    OnDidUpdatePrintableAreaCallback callback,
    const gfx::Rect& printable_area_um) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (printable_area_um.IsEmpty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  print_settings->UpdatePrinterPrintableArea(printable_area_um);
  std::move(callback).Run(/*success=*/true);
}
#endif

void PrinterQueryOop::SendEstablishPrintingContext(
    PrintBackendServiceManager::ClientId client_id,
    const std::string& printer_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ShouldPrintJobOop());

  DVLOG(1) << "Establishing printing context for system print";

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  content::WebContents* web_contents = GetWebContents();
  gfx::NativeView parent_view =
      web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr;
#endif

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  context_id_ = service_mgr.EstablishPrintingContext(client_id, printer_name
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
                                                     ,
                                                     parent_view
#endif
  );
}

void PrinterQueryOop::SendUseDefaultSettings(SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ShouldPrintJobOop());
  CHECK(query_with_ui_client_id_.has_value());

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.UseDefaultSettings(
      *query_with_ui_client_id_, *context_id_,
      base::BindOnce(&PrinterQueryOop::OnDidUseDefaultSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrinterQueryOop::SendAskUserForSettings(uint32_t document_page_count,
                                             bool has_selection,
                                             bool is_scripted,
                                             SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ShouldPrintJobOop());

  if (document_page_count > kMaxPageCount) {
    InvokeSettingsCallback(std::move(callback), mojom::ResultCode::kFailed);
    return;
  }

  content::WebContents* web_contents = GetWebContents();

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  if (web_contents && web_contents->IsFullscreen()) {
    web_contents->ExitFullscreen(true);
  }

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();
  service_mgr.AskUserForSettings(
      *query_with_ui_client_id_, *context_id_, document_page_count,
      has_selection, is_scripted,
      base::BindOnce(&PrinterQueryOop::OnDidAskUserForSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

std::unique_ptr<PrintJobWorkerOop> PrinterQueryOop::CreatePrintJobWorkerOop(
    PrintJob* print_job) {
  return std::make_unique<PrintJobWorkerOop>(
      std::move(printing_context_delegate_), std::move(printing_context_),
      print_document_client_id_, context_id_, print_job,
      print_from_system_dialog_);
}

}  // namespace printing
