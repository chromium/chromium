// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notifier.h"

#include <memory>

#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"

namespace policy {

namespace {

ui::DataTransferEndpoint CloneEndpoint(
    const ui::DataTransferEndpoint* const data_endpoint) {
  if (data_endpoint == nullptr)
    return ui::DataTransferEndpoint(ui::EndpointType::kDefault);

  return ui::DataTransferEndpoint(*data_endpoint);
}

void SynthesizePaste() {
  ui::KeyEvent control_press(/*type=*/ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                             /*code=*/static_cast<ui::DomCode>(0),
                             /*flags=*/0);
  if (!display::Screen::GetScreen())  // Doesn't exist in unittests.
    return;
  auto* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetDisplayForNewWindows().id());
  DCHECK(host);
  host->DeliverEventToSink(&control_press);

  ui::KeyEvent v_press(/*type=*/ui::ET_KEY_PRESSED, ui::VKEY_V,
                       /*code=*/static_cast<ui::DomCode>(0),
                       /*flags=*/ui::EF_CONTROL_DOWN);

  host->DeliverEventToSink(&v_press);

  ui::KeyEvent v_release(/*type=*/ui::ET_KEY_RELEASED, ui::VKEY_V,
                         /*code=*/static_cast<ui::DomCode>(0),
                         /*flags=*/ui::EF_CONTROL_DOWN);
  host->DeliverEventToSink(&v_release);

  ui::KeyEvent control_release(/*type=*/ui::ET_KEY_RELEASED, ui::VKEY_CONTROL,
                               /*code=*/static_cast<ui::DomCode>(0),
                               /*flags=*/0);
  host->DeliverEventToSink(&control_release);
}

}  // namespace

DlpClipboardNotifier::DlpClipboardNotifier() {
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
}

DlpClipboardNotifier::~DlpClipboardNotifier() {
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
}

void DlpClipboardNotifier::NotifyBlockedAction(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  DCHECK(data_src);
  DCHECK(data_src->origin());
  const base::string16 host_name =
      base::UTF8ToUTF16(data_src->origin()->host());
  if (data_dst) {
    if (data_dst->type() == ui::EndpointType::kCrostini) {
      ShowToast(kClipboardBlockCrostiniToastId,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
                    l10n_util::GetStringUTF16(IDS_CROSTINI_LINUX)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kPluginVm) {
      ShowToast(kClipboardBlockPluginVmToastId,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
                    l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kArc) {
      ShowToast(kClipboardBlockArcToastId,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
                    l10n_util::GetStringUTF16(IDS_POLICY_DLP_ANDROID_APPS)));
      return;
    }
  }

  ShowBlockBubble(l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_PASTE, host_name));
}

void DlpClipboardNotifier::WarnOnAction(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  DCHECK(data_src);
  DCHECK(data_src->origin());
  const base::string16 host_name =
      base::UTF8ToUTF16(data_src->origin()->host());

  if (data_dst) {
    if (data_dst->type() == ui::EndpointType::kCrostini) {
      ShowToast(kClipboardWarnCrostiniToastId,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_WARN_ON_COPY_VM,
                    l10n_util::GetStringUTF16(IDS_CROSTINI_LINUX)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kPluginVm) {
      ShowToast(kClipboardWarnPluginVmToastId,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_WARN_ON_COPY_VM,
                    l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kArc) {
      ShowToast(kClipboardWarnArcToastId,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_WARN_ON_COPY_VM,
                    l10n_util::GetStringUTF16(IDS_POLICY_DLP_ANDROID_APPS)));
      return;
    }
  }

  auto proceed_cb =
      base::BindRepeating(&DlpClipboardNotifier::ProceedOnWarn,
                          base::Unretained(this), CloneEndpoint(data_dst));
  ShowWarningBubble(l10n_util::GetStringFUTF16(
                        IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_PASTE, host_name),
                    proceed_cb);
}

bool DlpClipboardNotifier::DidUserProceedOnWarn(
    const ui::DataTransferEndpoint* const data_dst) {
  const ui::EndpointType dst_type =
      data_dst ? data_dst->type() : ui::EndpointType::kDefault;

  for (const auto& endpoint : approved_dsts_) {
    if (endpoint.type() == dst_type) {
      if (dst_type != ui::EndpointType::kUrl)
        return true;
      else if (endpoint.IsSameOriginWith(*data_dst))
        return true;
    }
  }
  return false;
}

void DlpClipboardNotifier::ProceedOnWarn(
    const ui::DataTransferEndpoint& data_dst,
    views::Widget* widget) {
  CloseWidget(widget, views::Widget::ClosedReason::kAcceptButtonClicked);
  approved_dsts_.push_back(data_dst);
  SynthesizePaste();
}

void DlpClipboardNotifier::ResetUserWarnSelection() {
  approved_dsts_.clear();
}

void DlpClipboardNotifier::ShowToast(const std::string& id,
                                     const base::string16& text) const {
  ash::ToastData toast(id, text, kClipboardDlpBlockDurationMs,
                       /*dismiss_text=*/base::nullopt);
  toast.is_managed = true;
  ash::ToastManager::Get()->Show(toast);
}

void DlpClipboardNotifier::OnClipboardDataChanged() {
  CloseWidget(widget_.get(), views::Widget::ClosedReason::kUnspecified);
  ResetUserWarnSelection();
}

}  // namespace policy
