// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notifier.h"

#include <memory>

#include "base/bind.h"
#include "base/notreached.h"
#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/policy/dlp/dlp_browser_helper_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetDisplayForNewWindows().id());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* host = dlp::GetActiveWindowTreeHost();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

bool HasEndpoint(const std::vector<ui::DataTransferEndpoint>& saved_endpoints,
                 const ui::DataTransferEndpoint* const endpoint) {
  const ui::EndpointType endpoint_type =
      endpoint ? endpoint->type() : ui::EndpointType::kDefault;

  for (const auto& ept : saved_endpoints) {
    if (ept.type() == endpoint_type) {
      if (endpoint_type != ui::EndpointType::kUrl)
        return true;
      else if (ept.IsSameURLWith(*endpoint))
        return true;
    }
  }
  return false;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void OnToastClicked() {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(dlp::kDlpLearnMoreUrl),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  DCHECK(data_src->GetURL());
  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (data_dst) {
    if (data_dst->type() == ui::EndpointType::kCrostini) {
      ShowToast(kClipboardBlockCrostiniToastId,
                ash::ToastCatalogName::kClipboardBlockedAction,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
                    l10n_util::GetStringUTF16(IDS_CROSTINI_LINUX)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kPluginVm) {
      ShowToast(kClipboardBlockPluginVmToastId,
                ash::ToastCatalogName::kClipboardBlockedAction,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
                    l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kArc) {
      ShowToast(kClipboardBlockArcToastId,
                ash::ToastCatalogName::kClipboardBlockedAction,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
                    l10n_util::GetStringUTF16(IDS_POLICY_DLP_ANDROID_APPS)));
      return;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  ShowBlockBubble(l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_PASTE, host_name));
}

void DlpClipboardNotifier::WarnOnPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    base::RepeatingCallback<void()> reporting_cb) {
  DCHECK(data_src);
  DCHECK(data_src->GetURL());

  CloseWidget(widget_.get(), views::Widget::ClosedReason::kUnspecified);

  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (data_dst) {
    if (data_dst->type() == ui::EndpointType::kCrostini) {
      ShowToast(kClipboardWarnCrostiniToastId,
                ash::ToastCatalogName::kClipboardWarnOnPaste,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_WARN_ON_COPY_VM,
                    l10n_util::GetStringUTF16(IDS_CROSTINI_LINUX)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kPluginVm) {
      ShowToast(kClipboardWarnPluginVmToastId,
                ash::ToastCatalogName::kClipboardWarnOnPaste,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_WARN_ON_COPY_VM,
                    l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kArc) {
      ShowToast(kClipboardWarnArcToastId,
                ash::ToastCatalogName::kClipboardWarnOnPaste,
                l10n_util::GetStringFUTF16(
                    IDS_POLICY_DLP_CLIPBOARD_WARN_ON_COPY_VM,
                    l10n_util::GetStringUTF16(IDS_POLICY_DLP_ANDROID_APPS)));
      return;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto proceed_cb = base::BindRepeating(
      &DlpClipboardNotifier::ProceedPressed, base::Unretained(this),
      CloneEndpoint(data_dst), std::move(reporting_cb));
  auto cancel_cb =
      base::BindRepeating(&DlpClipboardNotifier::CancelWarningPressed,
                          base::Unretained(this), CloneEndpoint(data_dst));

  ShowWarningBubble(l10n_util::GetStringFUTF16(
                        IDS_POLICY_DLP_CLIPBOARD_WARN_ON_PASTE, host_name),
                    std::move(proceed_cb), std::move(cancel_cb));
}

void DlpClipboardNotifier::WarnOnBlinkPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> paste_cb) {
  DCHECK(data_src);
  DCHECK(data_src->GetURL());

  CloseWidget(widget_.get(), views::Widget::ClosedReason::kUnspecified);

  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());

  auto proceed_cb =
      base::BindRepeating(&DlpClipboardNotifier::BlinkProceedPressed,
                          base::Unretained(this), CloneEndpoint(data_dst));
  auto cancel_cb =
      base::BindRepeating(&DlpClipboardNotifier::CancelWarningPressed,
                          base::Unretained(this), CloneEndpoint(data_dst));

  ShowWarningBubble(l10n_util::GetStringFUTF16(
                        IDS_POLICY_DLP_CLIPBOARD_WARN_ON_PASTE, host_name),
                    std::move(proceed_cb), std::move(cancel_cb));
  SetPasteCallback(std::move(paste_cb));
  Observe(web_contents);
}

bool DlpClipboardNotifier::DidUserApproveDst(
    const ui::DataTransferEndpoint* const data_dst) {
  return HasEndpoint(approved_dsts_, data_dst);
}

bool DlpClipboardNotifier::DidUserCancelDst(
    const ui::DataTransferEndpoint* const data_dst) {
  return HasEndpoint(cancelled_dsts_, data_dst);
}

void DlpClipboardNotifier::ProceedPressed(
    const ui::DataTransferEndpoint& data_dst,
    base::RepeatingCallback<void()> reporting_cb,
    views::Widget* widget) {
  CloseWidget(widget, views::Widget::ClosedReason::kAcceptButtonClicked);
  approved_dsts_.push_back(data_dst);
  SynthesizePaste();
  std::move(reporting_cb).Run();
}

void DlpClipboardNotifier::BlinkProceedPressed(
    const ui::DataTransferEndpoint& data_dst,
    views::Widget* widget) {
  approved_dsts_.push_back(data_dst);
  RunPasteCallback();
  CloseWidget(widget, views::Widget::ClosedReason::kAcceptButtonClicked);
}

void DlpClipboardNotifier::CancelWarningPressed(
    const ui::DataTransferEndpoint& data_dst,
    views::Widget* widget) {
  cancelled_dsts_.push_back(data_dst);
  CloseWidget(widget, views::Widget::ClosedReason::kCancelButtonClicked);
}

void DlpClipboardNotifier::ResetUserWarnSelection() {
  approved_dsts_.clear();
  cancelled_dsts_.clear();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void DlpClipboardNotifier::ShowToast(const std::string& id,
                                     ash::ToastCatalogName catalog_name,
                                     const std::u16string& text) const {
  ash::ToastData toast(
      id, catalog_name, text, ash::ToastData::kDefaultToastDuration,
      /*visible_on_lock_screen=*/false,
      /*has_dismiss_button=*/true,
      /*custom_dismiss_text=*/
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_CLIPBOARD_BLOCK_TOAST_BUTTON));
  toast.is_managed = true;
  toast.dismiss_callback = base::BindRepeating(&OnToastClicked);
  ash::ToastManager::Get()->Show(std::move(toast));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void DlpClipboardNotifier::OnClipboardDataChanged() {
  ResetUserWarnSelection();
}

void DlpClipboardNotifier::OnWidgetDestroying(views::Widget* widget) {
  Observe(nullptr);
  DlpDataTransferNotifier::OnWidgetDestroying(widget);
}

void DlpClipboardNotifier::WebContentsDestroyed() {
  CloseWidget(widget_.get(), views::Widget::ClosedReason::kUnspecified);
}

}  // namespace policy
