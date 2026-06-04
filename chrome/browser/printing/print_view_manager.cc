// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/pref_names.h"
#include "components/device_event_log/device_event_log.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/oop_features.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/printing/print_job_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#endif

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/data_protection/print_utils.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

PrintManager* g_receiver_for_testing = nullptr;

// Keeps track of pending scripted print preview closures.
using ScriptedPrintPreviewClosureMap =
    std::map<content::RenderProcessHost*, base::OnceClosure>;

ScriptedPrintPreviewClosureMap& GetScriptedPrintPreviewClosureMap() {
  // No locking, only access on the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<ScriptedPrintPreviewClosureMap> closure_map;
  return *closure_map;
}

void OnScriptedPrintPreviewReply(
    PrintViewManager::SetupScriptedPrintPreviewCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run();
}

std::string PrintMsgPrintParamsErrorDetails(const mojom::PrintParams& params) {
  std::vector<std::string_view> details;

  if (params.content_size.IsEmpty()) {
    details.push_back("content size is empty");
  }
  if (params.page_size.IsEmpty()) {
    details.push_back("page size is empty");
  }
  if (params.printable_area.IsEmpty()) {
    details.push_back("printable area is empty");
  }
  if (!params.document_cookie) {
    details.push_back("invalid document cookie");
  }
  if (params.dpi.width() <= kMinDpi || params.dpi.height() <= kMinDpi) {
    details.push_back("invalid DPI dimensions");
  }
  if (params.margin_top < 0 || params.margin_left < 0) {
    details.push_back("invalid margins");
  }

  return base::JoinString(details, "; ");
}

}  // namespace

PrintViewManager::PrintViewManager(content::WebContents* web_contents)
    : PrintViewManagerBase(web_contents),
      content::WebContentsUserData<PrintViewManager>(*web_contents) {}

PrintViewManager::~PrintViewManager() {
  DCHECK_EQ(NOT_PREVIEWING, print_preview_state_);
}

// static
void PrintViewManager::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  if (g_receiver_for_testing) {
    g_receiver_for_testing->BindReceiver(std::move(receiver), rfh);
    return;
  }

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* print_manager = PrintViewManager::FromWebContents(web_contents);
  if (!print_manager)
    return;
  print_manager->BindReceiver(std::move(receiver), rfh);
}

bool PrintViewManager::PrintForSystemDialogNow(
    base::OnceClosure dialog_shown_callback) {
  DCHECK(dialog_shown_callback);
  DCHECK(!on_print_dialog_shown_callback_);
  on_print_dialog_shown_callback_ = std::move(dialog_shown_callback);
  is_switching_to_system_dialog_ = true;

  // Remember the ID for `print_preview_rfh_`, to enable checking that the
  // `RenderFrameHost` is still valid after a possible inner message loop runs
  // in `DisconnectFromCurrentPrintJob()`.
  content::GlobalRenderFrameHostId rfh_id = print_preview_rfh_->GetGlobalId();

  auto weak_this = weak_factory_.GetWeakPtr();
  DisconnectFromCurrentPrintJob();
  if (!weak_this)
    return false;

  // Don't print / print preview crashed tabs.
  if (IsCrashed())
    return false;

  // Don't print if `print_preview_rfh_` is no longer live.
  if (!content::RenderFrameHost::FromID(rfh_id) ||
      !print_preview_rfh_->IsRenderFrameLive()) {
    return false;
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (ShouldPrintJobOop()) {
    // Register this worker so that the service persists as long as the user
    // keeps the system print dialog UI displayed.
    if (!RegisterSystemPrintClient())
      return false;
  }
#endif

  SetPrintingRFH(print_preview_rfh_);
  PrintForSystemDialogImpl();
  return true;
}

bool PrintViewManager::BasicPrint(content::RenderFrameHost* rfh) {
  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  content::WebContents* print_preview_dialog =
      dialog_controller->GetPrintPreviewForContents(web_contents());
  if (!print_preview_dialog)
    return PrintNow(rfh);

  return !!print_preview_dialog->GetWebUI();
}

bool PrintViewManager::PrintPreviewNow(content::RenderFrameHost* rfh,
                                       bool has_selection) {
  return PrintPreview(rfh,
#if BUILDFLAG(IS_CHROMEOS)
                      mojo::NullAssociatedRemote(),
#endif
                      has_selection);
}

#if BUILDFLAG(IS_CHROMEOS)
bool PrintViewManager::PrintPreviewWithPrintRenderer(
    content::RenderFrameHost* rfh,
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer) {
  return PrintPreview(rfh, std::move(print_renderer), /*has_selection=*/false);
}
#endif

void PrintViewManager::PrintPreviewForNodeUnderContextMenu(
    content::RenderFrameHost* rfh) {
  if (print_preview_state_ != NOT_PREVIEWING) {
    return;
  }

  // Don't print / print preview crashed tabs.
  if (IsCrashed() || !rfh->IsRenderFrameLive()) {
    return;
  }

  // This will indirectly trigger PrintPreviewForWebNode() below, which sets
  // `print_preview_state_`.
  GetPrintRenderFrame(rfh)->PrintNodeUnderContextMenu();
}

void PrintViewManager::PrintPreviewForWebNode(content::RenderFrameHost* rfh) {
  if (print_preview_state_ != NOT_PREVIEWING)
    return;

  SetPrintPreviewRenderFrameHost(rfh);
  print_preview_state_ = USER_INITIATED_PREVIEW;
}

void PrintViewManager::PrintPreviewAlmostDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (print_preview_state_ != SCRIPTED_PREVIEW)
    return;

  MaybeUnblockScriptedPreviewRPH();
}

void PrintViewManager::PrintPreviewDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (print_preview_state_ == NOT_PREVIEWING)
    return;

// Send OnPrintPreviewDialogClosed message for 'afterprint' event.
#if BUILDFLAG(IS_WIN)
  // On Windows, we always send OnPrintPreviewDialogClosed. It's ok to dispatch
  // 'afterprint' at this timing because system dialog printing on
  // Windows doesn't need the original frame.
  bool send_message = true;
#else
  // On non-Windows, we don't need to send OnPrintPreviewDialogClosed when we
  // are switching to system dialog. PrintRenderFrameHelper is responsible to
  // dispatch 'afterprint' event.
  bool send_message = !is_switching_to_system_dialog_;
#endif
  if (send_message) {
    // Only send a message about having closed if the RenderFrame is live and
    // PrintRenderFrame is connected. Normally IsPrintRenderFrameConnected()
    // implies  IsRenderFrameLive(). However, when a renderer process exits
    // (e.g. due to a crash), RenderFrameDeleted() and PrintPreviewDone() are
    // triggered by independent observers. Since there is no guarantee which
    // observer will run first, both conditions are explicitly checked here.
    if (print_preview_rfh_->IsRenderFrameLive() &&
        IsPrintRenderFrameConnected(print_preview_rfh_)) {
      GetPrintRenderFrame(print_preview_rfh_)->OnPrintPreviewDialogClosed();
    }
  }
  is_switching_to_system_dialog_ = false;

  if (print_preview_state_ == SCRIPTED_PREVIEW) {
    auto& map = GetScriptedPrintPreviewClosureMap();
    auto it = map.find(scripted_print_preview_rph_);
    CHECK(it != map.end());
    std::move(it->second).Run();
    map.erase(it);

    // PrintPreviewAlmostDone() usually already calls this. Calling it again
    // will likely be a no-op, but do it anyway to reset the state for sure.
    MaybeUnblockScriptedPreviewRPH();
    scripted_print_preview_rph_ = nullptr;
  }
  ClearPrintPreviewSettings();
  print_preview_state_ = NOT_PREVIEWING;
  print_preview_rfh_ = nullptr;
  for (auto& observer : GetTestObservers()) {
    observer.OnPrintPreviewDone();
  }
}

void PrintViewManager::AppendPrintPreviewSettings(base::DictValue settings,
                                                  bool is_pdf) {
  CHECK(!settings.empty());
  if (is_pdf) {
    settings.Set(kSettingHeaderFooterEnabled, false);
    settings.Set(kSettingMarginsType,
                 static_cast<int>(mojom::MarginType::kNoMargins));
  }
  print_preview_settings_.push(std::move(settings));
}

void PrintViewManager::ClearPrintPreviewSettings() {
  while (!print_preview_settings_.empty()) {
    print_preview_settings_.pop();
  }
}

void PrintViewManager::RejectPrintPreviewRequestIfRestricted(
    content::GlobalRenderFrameHostId rfh_id,
    base::OnceCallback<void(bool should_proceed)> callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // Don't print DLP restricted content on Chrome OS, and use `callback`
  // directly since scanning isn't an option.
  policy::DlpContentManager::Get()->CheckPrintingRestriction(
      web_contents(), rfh_id, std::move(callback));
#else
  std::move(callback).Run(/*should_proceed=*/true);
#endif
}

void PrintViewManager::OnPrintPreviewRequestRejected(
    content::GlobalRenderFrameHostId rfh_id) {
  if (!content::RenderFrameHost::FromID(rfh_id))
    return;

  PrintPreviewDone();
  PrintPreviewRejectedForTesting();
}

void PrintViewManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == print_preview_rfh_)
    PrintPreviewDone();
  PrintViewManagerBase::RenderFrameDeleted(render_frame_host);
}

// static
void PrintViewManager::SetReceiverImplForTesting(PrintManager* impl) {
  g_receiver_for_testing = impl;
}

bool PrintViewManager::PrintPreview(
    content::RenderFrameHost* rfh,
#if BUILDFLAG(IS_CHROMEOS)
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
#endif
    bool has_selection) {
  // Users can send print commands all they want and it is beyond
  // PrintViewManager's control. Just ignore the extra commands.
  // See http://crbug.com/40240300 for example.
  if (print_preview_state_ != NOT_PREVIEWING) {
    return false;
  }

  // Don't print / print preview crashed tabs.
  if (IsCrashed() || !rfh->IsRenderFrameLive()) {
    return false;
  }

  // Don't print / print preview fenched frames.
  if (rfh->IsNestedWithinFencedFrame()) {
    return false;
  }

  GetPrintRenderFrame(rfh)->InitiatePrintPreview(
#if BUILDFLAG(IS_CHROMEOS)
      std::move(print_renderer),
#endif
      has_selection);

  SetPrintPreviewRenderFrameHost(rfh);
  print_preview_state_ = USER_INITIATED_PREVIEW;
  return true;
}

void PrintViewManager::DidShowPrintDialog() {
  if (GetCurrentTargetFrame() != print_preview_rfh_)
    return;

  if (on_print_dialog_shown_callback_)
    std::move(on_print_dialog_shown_callback_).Run();
}

void PrintViewManager::GetPrintPreviewParams(
    GetPrintPreviewParamsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!GetPrintingEnabledBooleanPref()) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (print_preview_settings_.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  base::DictValue job_settings = std::move(print_preview_settings_.front());
  print_preview_settings_.pop();
  CHECK(!job_settings.empty());

  std::optional<int> printer_type_value =
      job_settings.FindInt(kSettingPrinterType);
  if (!printer_type_value) {
    std::move(callback).Run(nullptr);
    return;
  }

  mojom::PrinterType printer_type =
      static_cast<mojom::PrinterType>(*printer_type_value);
  if (printer_type != mojom::PrinterType::kExtension &&
      printer_type != mojom::PrinterType::kPdf &&
      printer_type != mojom::PrinterType::kLocal) {
    std::move(callback).Run(nullptr);
    return;
  }

  // `job_settings` does not yet contain the rasterized PDF dpi, so if the user
  // has the print preference set, fetch it for use in
  // `PrintSettingsFromJobSettings()`.
  content::BrowserContext* context =
      web_contents() ? web_contents()->GetBrowserContext() : nullptr;
  PrefService* prefs =
      context ? Profile::FromBrowserContext(context)->GetPrefs() : nullptr;
  if (prefs && prefs->HasPrefPath(prefs::kPrintRasterizePdfDpi)) {
    int value = prefs->GetInteger(prefs::kPrintRasterizePdfDpi);
    if (value > 0) {
      job_settings.Set(kSettingRasterizePdfDpi, value);
    }
  }

  std::unique_ptr<PrintSettings> print_settings =
      PrintSettingsFromJobSettings(job_settings);
  if (!print_settings) {
    std::move(callback).Run(nullptr);
    return;
  }

  bool open_in_external_preview =
      job_settings.contains(kSettingOpenPDFInPreview);
  if (!open_in_external_preview &&
      (printer_type == mojom::PrinterType::kPdf ||
       printer_type == mojom::PrinterType::kExtension)) {
    if (print_settings->page_setup_device_units().printable_area().IsEmpty()) {
      PrinterQuery::ApplyDefaultPrintableAreaToVirtualPrinterPrintSettings(
          *print_settings);
    }
  }

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40260379):  Remove this if the printable areas can be made
  // fully available from `PrintBackend::GetPrinterSemanticCapsAndDefaults()`
  // for in-browser queries.
  if (printer_type == mojom::PrinterType::kLocal) {
    // Without a document cookie to find a previous query, must generate a
    // fresh printer query each time, even if the paper size didn't change.
    std::unique_ptr<PrinterQuery> printer_query =
        queue()->CreatePrinterQuery(GetCurrentTargetFrame()->GetGlobalId());

    auto* printer_query_ptr = printer_query.get();
    auto* print_settings_ptr = print_settings.get();
    printer_query_ptr->UpdatePrintableArea(
        print_settings_ptr,
        base::BindOnce(&PrintViewManager::OnDidUpdatePrintableArea,
                       weak_factory_.GetWeakPtr(), std::move(printer_query),
                       std::move(job_settings), std::move(print_settings),
                       std::move(callback)));
    return;
  }
#endif

  CompleteGetPrintPreviewParams(std::move(job_settings),
                                std::move(print_settings), std::move(callback));
}

void PrintViewManager::SetupScriptedPrintPreview(
    SetupScriptedPrintPreviewCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderFrameHost* rfh = GetCurrentTargetFrame();
  // The Mojo receiver endpoint is owned by a RenderFrameHostReceiverSet, so
  // this DCHECK should always hold.
  DCHECK(rfh->IsRenderFrameLive());
  content::RenderProcessHost* rph = rfh->GetProcess();

  if (rfh->IsNestedWithinFencedFrame()) {
    // The renderer should have checked and disallowed the request for fenced
    // frames in ChromeClient. Ignore the request and mark it as bad if it
    // didn't happen for some reason.
    bad_message::ReceivedBadMessage(
        rph, bad_message::PVM_SCRIPTED_PRINT_FENCED_FRAME);
    std::move(callback).Run();
    return;
  }

  if (!rfh->IsActive()) {
    // Only active RFHs should show UI elements.
    std::move(callback).Run();
    return;
  }

  auto& map = GetScriptedPrintPreviewClosureMap();
  if (map.contains(rph)) {
    // Renderer already handling window.print(). Abort this attempt to prevent
    // the renderer from having multiple nested loops. If multiple nested loops
    // existed, then they have to exit in the right order and that is messy.
    std::move(callback).Run();
    return;
  }

  if (print_preview_state_ != NOT_PREVIEWING) {
    // If a print dialog is already open for this tab, ignore the scripted print
    // message.
    std::move(callback).Run();
    return;
  }

  // Since window.print() is renderer-initiated, explicitly establish a
  // connection to the RenderFrame here. Without this, later operations that
  // expect the established connection can unexpected fail.
  GetPrintRenderFrame(rfh);

  SetPrintPreviewRenderFrameHost(rfh);
  print_preview_state_ = SCRIPTED_PREVIEW;
  map[rph] = base::BindOnce(&OnScriptedPrintPreviewReply, std::move(callback));
  scripted_print_preview_rph_ = rph;
  DCHECK(!scripted_print_preview_rph_set_blocked_);
  if (!scripted_print_preview_rph_->IsBlocked()) {
    scripted_print_preview_rph_->SetBlocked(true);
    scripted_print_preview_rph_set_blocked_ = true;
  }
}

void PrintViewManager::ShowScriptedPrintPreview() {
  if (print_preview_state_ != SCRIPTED_PREVIEW) {
    return;
  }

  DCHECK(print_preview_rfh_);
  if (GetCurrentTargetFrame() != print_preview_rfh_)
    return;
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  set_analyzing_content(/*analyzing=*/true);
#endif
  RejectPrintPreviewRequestIfRestricted(
      print_preview_rfh_->GetGlobalId(),
      base::BindOnce(&PrintViewManager::OnScriptedPrintPreviewCallback,
                     weak_factory_.GetWeakPtr(),
                     print_preview_rfh_->GetGlobalId()));
}

void PrintViewManager::OnScriptedPrintPreviewCallback(
    content::GlobalRenderFrameHostId rfh_id,
    bool should_proceed) {
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  set_analyzing_content(/*analyzing=*/false);
#endif
  if (!should_proceed) {
    OnPrintPreviewRequestRejected(rfh_id);
    return;
  }

  if (print_preview_state_ != SCRIPTED_PREVIEW) {
    return;
  }

  DCHECK(print_preview_rfh_);

  auto* rfh = content::RenderFrameHost::FromID(rfh_id);
  if (!rfh || rfh != print_preview_rfh_ || !print_preview_rfh_->IsActive()) {
    return;
  }

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // https://crbug.com/41322524
  if (web_contents()->IsFullscreen()) {
    // Return early if `this` got destroyed inside ExitFullscreen().
    // https://crbug.com/517047197
    auto weak_this = weak_factory_.GetWeakPtr();
    web_contents()->ExitFullscreen(true);
    if (!weak_this) {
      return;
    }
  }

  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  mojom::RequestPrintPreviewParams params;
  params.is_modifiable = !print_preview_rfh_->GetProcess()->IsPdf();
  dialog_controller->PrintPreview(web_contents(), params);

  PrintPreviewAllowedForTesting();
}

void PrintViewManager::RequestPrintPreview(
    mojom::RequestPrintPreviewParamsPtr params) {
  auto* rfh = GetCurrentTargetFrame();
  if (rfh->IsNestedWithinFencedFrame()) {
    // Either the renderer should have checked and disallowed the request for
    // fenced frames in ChromeClient, or PrintPreview() above should have
    // checked. Ignore the request and mark it as bad if those checks didn't
    // happen for some reason.
    bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                    bad_message::PVM_PRINT_FENCED_FRAME);
    return;
  }

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  set_analyzing_content(/*analyzing=*/true);
#endif
  RejectPrintPreviewRequestIfRestricted(
      rfh->GetGlobalId(),
      base::BindOnce(&PrintViewManager::OnRequestPrintPreviewCallback,
                     weak_factory_.GetWeakPtr(), std::move(params),
                     GetCurrentTargetFrame()->GetGlobalId()));
}

void PrintViewManager::OnRequestPrintPreviewCallback(
    mojom::RequestPrintPreviewParamsPtr params,
    content::GlobalRenderFrameHostId rfh_id,
    bool should_proceed) {
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  set_analyzing_content(/*analyzing=*/false);
#endif
  if (!should_proceed) {
    OnPrintPreviewRequestRejected(rfh_id);
    return;
  }

  // Double-check that the RenderFrameHost is still alive and has a live
  // RenderFrame, since the DLP check is potentially asynchronous.
  auto* render_frame_host = content::RenderFrameHost::FromID(rfh_id);
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive()) {
    return;
  }

  // Also check it is active. Only active RFHs should show UI elements.
  if (!render_frame_host->IsActive()) {
    return;
  }

  if (params->webnode_only) {
    PrintPreviewForWebNode(render_frame_host);
  }

  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  dialog_controller->PrintPreview(web_contents(), *params);

  PrintPreviewAllowedForTesting();
}

void PrintViewManager::CheckForCancel(int32_t preview_ui_id,
                                      int32_t request_id,
                                      CheckForCancelCallback callback) {
  std::move(callback).Run(
      PrintPreviewUI::ShouldCancelRequest(preview_ui_id, request_id));
}

void PrintViewManager::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {
  auto* client = PrintCompositeClient::FromWebContents(web_contents());
  if (client) {
    client->SetAccessibilityTree(cookie, accessibility_tree);
  }
}

void PrintViewManager::MaybeUnblockScriptedPreviewRPH() {
  if (scripted_print_preview_rph_set_blocked_) {
    scripted_print_preview_rph_->SetBlocked(false);
    scripted_print_preview_rph_set_blocked_ = false;
  }
}

void PrintViewManager::SetPrintPreviewRenderFrameHost(
    content::RenderFrameHost* rfh) {
  DCHECK_EQ(print_preview_state_, NOT_PREVIEWING);
  DCHECK(rfh);
  DCHECK(IsPrintRenderFrameConnected(rfh));
  // All callers should already ensure this condition holds; CHECK to
  // aggressively protect against future unsafety.
  CHECK(rfh->IsRenderFrameLive());
  DCHECK(!print_preview_rfh_);
  print_preview_rfh_ = rfh;
}

void PrintViewManager::PrintForSystemDialogImpl() {
  GetPrintRenderFrame(print_preview_rfh_)->PrintForSystemDialog();
}

#if BUILDFLAG(IS_WIN)
void PrintViewManager::OnDidUpdatePrintableArea(
    std::unique_ptr<PrinterQuery> printer_query,
    base::DictValue job_settings,
    std::unique_ptr<PrintSettings> print_settings,
    GetPrintPreviewParamsCallback callback,
    bool success) {
  if (!success) {
    PRINTER_LOG(ERROR) << "Unable to update printable area for "
                       << base::UTF16ToUTF8(print_settings->device_name())
                       << " (paper vendor id "
                       << print_settings->requested_media().vendor_id << ")";
    std::move(callback).Run(nullptr);
    return;
  }
  PRINTER_LOG(EVENT) << "Paper printable area updated for vendor id "
                     << print_settings->requested_media().vendor_id;
  CompleteGetPrintPreviewParams(std::move(job_settings),
                                std::move(print_settings), std::move(callback));
}
#endif

void PrintViewManager::CompleteGetPrintPreviewParams(
    base::DictValue job_settings,
    std::unique_ptr<PrintSettings> print_settings,
    GetPrintPreviewParamsCallback callback) {
  mojom::PrintPagesParamsPtr settings = mojom::PrintPagesParams::New();
  settings->pages = GetPageRangesFromJobSettings(job_settings);
  settings->params = mojom::PrintParams::New();
  RenderParamsFromPrintSettings(*print_settings, settings->params.get());
  settings->params->document_cookie = PrintSettings::NewCookie();
  if (!PrintMsgPrintParamsIsValid(*settings->params)) {
    mojom::PrinterType printer_type = static_cast<mojom::PrinterType>(
        *job_settings.FindInt(kSettingPrinterType));
    PRINTER_LOG(ERROR) << "Printer settings invalid for "
                       << base::UTF16ToUTF8(print_settings->device_name())
                       << " (destination type " << printer_type << "): "
                       << PrintMsgPrintParamsErrorDetails(*settings->params);
    std::move(callback).Run(nullptr);
    return;
  }

  set_cookie(settings->params->document_cookie);
  std::move(callback).Run(std::move(settings));
}

void PrintViewManager::PrintPreviewRejectedForTesting() {
  // Note: This is only used for testing.
}

void PrintViewManager::PrintPreviewAllowedForTesting() {
  // Note: This is only used for testing.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManager);

}  // namespace printing
