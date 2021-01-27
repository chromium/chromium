// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

// Keeps track of pending scripted print preview closures.
// No locking, only access on the UI thread.
base::LazyInstance<std::map<content::RenderProcessHost*, base::OnceClosure>>::
    Leaky g_scripted_print_preview_closure_map = LAZY_INSTANCE_INITIALIZER;

void EnableInternalPDFPluginForContents(int render_process_id,
                                        int render_frame_id) {
  // Always enable the internal PDF plugin for the print preview page.
  static const base::NoDestructor<base::FilePath> pdf_plugin_path(
      ChromeContentClient::kPDFPluginPath);
  auto* plugin_service = content::PluginService::GetInstance();
  const content::PepperPluginInfo* info =
      plugin_service->GetRegisteredPpapiPluginInfo(*pdf_plugin_path);
  if (!info)
    return;

  ChromePluginServiceFilter::GetInstance()->OverridePluginForFrame(
      render_process_id, render_frame_id, info->ToWebPluginInfo());
}

content::WebContents* GetPrintPreviewDialog(
    content::WebContents* web_contents) {
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return nullptr;
  return dialog_controller->GetPrintPreviewForContents(web_contents);
}

}  // namespace

PrintViewManager::PrintViewManager(content::WebContents* web_contents)
    : PrintViewManagerBase(web_contents) {
  if (PrintPreviewDialogController::IsPrintPreviewURL(web_contents->GetURL())) {
    EnableInternalPDFPluginForContents(
        web_contents->GetMainFrame()->GetProcess()->GetID(),
        web_contents->GetMainFrame()->GetRoutingID());
  }
}

PrintViewManager::~PrintViewManager() {
  DCHECK_EQ(NOT_PREVIEWING, print_preview_state_);
}

bool PrintViewManager::PrintForSystemDialogNow(
    base::OnceClosure dialog_shown_callback) {
  DCHECK(dialog_shown_callback);
  DCHECK(!on_print_dialog_shown_callback_);
  on_print_dialog_shown_callback_ = std::move(dialog_shown_callback);
  is_switching_to_system_dialog_ = true;
  DisconnectFromCurrentPrintJob();

  // Don't print / print preview crashed tabs.
  if (IsCrashed())
    return false;

  SetPrintingRFH(print_preview_rfh_);
  GetPrintRenderFrame(print_preview_rfh_)->PrintForSystemDialog();
  return true;
}

bool PrintViewManager::BasicPrint(content::RenderFrameHost* rfh) {
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return false;

  content::WebContents* print_preview_dialog =
      dialog_controller->GetPrintPreviewForContents(web_contents());
  if (!print_preview_dialog)
    return PrintNow(rfh);

  return !!print_preview_dialog->GetWebUI();
}

bool PrintViewManager::PrintPreviewNow(content::RenderFrameHost* rfh,
                                       bool has_selection) {
  return PrintPreview(rfh, mojo::NullAssociatedRemote(), has_selection);
}

bool PrintViewManager::PrintPreviewWithPrintRenderer(
    content::RenderFrameHost* rfh,
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer) {
  return PrintPreview(rfh, std::move(print_renderer), false);
}

void PrintViewManager::PrintPreviewForWebNode(content::RenderFrameHost* rfh) {
  if (print_preview_state_ != NOT_PREVIEWING)
    return;

  DCHECK(rfh);
  DCHECK(!print_preview_rfh_);
  print_preview_rfh_ = rfh;
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
#if defined(OS_WIN)
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
    // Only send a message about having closed if it is still connected.
    if (IsPrintRenderFrameConnected(print_preview_rfh_))
      GetPrintRenderFrame(print_preview_rfh_)->OnPrintPreviewDialogClosed();
  }
  is_switching_to_system_dialog_ = false;

  if (print_preview_state_ == SCRIPTED_PREVIEW) {
    auto& map = g_scripted_print_preview_closure_map.Get();
    auto it = map.find(scripted_print_preview_rph_);
    CHECK(it != map.end());
    std::move(it->second).Run();
    map.erase(it);

    // PrintPreviewAlmostDone() usually already calls this. Calling it again
    // will likely be a no-op, but do it anyway to reset the state for sure.
    MaybeUnblockScriptedPreviewRPH();
    scripted_print_preview_rph_ = nullptr;
  }
  print_preview_state_ = NOT_PREVIEWING;
  print_preview_rfh_ = nullptr;
}

bool PrintViewManager::RejectPrintPreviewRequestIfRestricted(
    content::RenderFrameHost* rfh) {
  if (!IsPrintingRestricted())
    return false;
  GetPrintRenderFrame(rfh)->OnPrintPreviewDialogClosed();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::ShowDlpPrintDisabledNotification();
#endif
  return true;
}

void PrintViewManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (PrintPreviewDialogController::IsPrintPreviewURL(
          web_contents()->GetURL())) {
    EnableInternalPDFPluginForContents(render_frame_host->GetProcess()->GetID(),
                                       render_frame_host->GetRoutingID());
  }
}

void PrintViewManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == print_preview_rfh_)
    PrintPreviewDone();
  PrintViewManagerBase::RenderFrameDeleted(render_frame_host);
}

bool PrintViewManager::PrintPreview(
    content::RenderFrameHost* rfh,
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
    bool has_selection) {
  // Users can send print commands all they want and it is beyond
  // PrintViewManager's control. Just ignore the extra commands.
  // See http://crbug.com/136842 for example.
  if (print_preview_state_ != NOT_PREVIEWING)
    return false;

  // Don't print / print preview crashed tabs.
  if (IsCrashed())
    return false;

  if (IsPrintingRestricted()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    policy::ShowDlpPrintDisabledNotification();
#endif
    return false;
  }

  GetPrintRenderFrame(rfh)->InitiatePrintPreview(std::move(print_renderer),
                                                 has_selection);

  DCHECK(!print_preview_rfh_);
  print_preview_rfh_ = rfh;
  print_preview_state_ = USER_INITIATED_PREVIEW;
  return true;
}

void PrintViewManager::DidShowPrintDialog() {
  if (print_manager_host_receivers_.GetCurrentTargetFrame() !=
      print_preview_rfh_) {
    return;
  }

  if (on_print_dialog_shown_callback_)
    std::move(on_print_dialog_shown_callback_).Run();
}

void PrintViewManager::SetupScriptedPrintPreview(
    SetupScriptedPrintPreviewCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& map = g_scripted_print_preview_closure_map.Get();
  content::RenderFrameHost* rfh =
      print_manager_host_receivers_.GetCurrentTargetFrame();
  content::RenderProcessHost* rph = rfh->GetProcess();

  if (base::Contains(map, rph)) {
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

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller) {
    std::move(callback).Run();
    return;
  }

  if (RejectPrintPreviewRequestIfRestricted(rfh)) {
    std::move(callback).Run();
    return;
  }

  DCHECK(!print_preview_rfh_);
  print_preview_rfh_ = rfh;
  print_preview_state_ = SCRIPTED_PREVIEW;
  map[rph] = base::BindOnce(&PrintViewManager::OnScriptedPrintPreviewReply,
                            base::Unretained(this), std::move(callback));
  scripted_print_preview_rph_ = rph;
  DCHECK(!scripted_print_preview_rph_set_blocked_);
  if (!scripted_print_preview_rph_->IsBlocked()) {
    scripted_print_preview_rph_->SetBlocked(true);
    scripted_print_preview_rph_set_blocked_ = true;
  }
}

void PrintViewManager::ShowScriptedPrintPreview(bool source_is_modifiable) {
  if (print_preview_state_ != SCRIPTED_PREVIEW)
    return;

  DCHECK(print_preview_rfh_);
  if (print_manager_host_receivers_.GetCurrentTargetFrame() !=
      print_preview_rfh_)
    return;

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller) {
    PrintPreviewDone();
    return;
  }

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  if (web_contents()->IsFullscreen())
    web_contents()->ExitFullscreen(true);

  dialog_controller->PrintPreview(web_contents());
  mojom::RequestPrintPreviewParams params;
  params.is_modifiable = source_is_modifiable;
  PrintPreviewUI::SetInitialParams(
      dialog_controller->GetPrintPreviewForContents(web_contents()), params);
}

void PrintViewManager::RequestPrintPreview(
    mojom::RequestPrintPreviewParamsPtr params) {
  content::RenderFrameHost* render_frame_host =
      print_manager_host_receivers_.GetCurrentTargetFrame();

  if (RejectPrintPreviewRequestIfRestricted(render_frame_host))
    return;

  if (params->webnode_only) {
    PrintPreviewForWebNode(render_frame_host);
  }
  PrintPreviewDialogController::PrintPreview(web_contents());
  PrintPreviewUI::SetInitialParams(GetPrintPreviewDialog(web_contents()),
                                   *params);
}

void PrintViewManager::CheckForCancel(int32_t preview_ui_id,
                                      int32_t request_id,
                                      CheckForCancelCallback callback) {
  std::move(callback).Run(
      PrintPreviewUI::ShouldCancelRequest(preview_ui_id, request_id));
}

void PrintViewManager::OnScriptedPrintPreviewReply(
    SetupScriptedPrintPreviewCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run();
}

void PrintViewManager::MaybeUnblockScriptedPreviewRPH() {
  if (scripted_print_preview_rph_set_blocked_) {
    scripted_print_preview_rph_->SetBlocked(false);
    scripted_print_preview_rph_set_blocked_ = false;
  }
}

bool PrintViewManager::IsPrintingRestricted() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Don't print DLP restricted content on Chrome OS.
  return policy::DlpContentManager::Get()->IsPrintingRestricted(web_contents());
#else
  return false;
#endif
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManager)

}  // namespace printing
