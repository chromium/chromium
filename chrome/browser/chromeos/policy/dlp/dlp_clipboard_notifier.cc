// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notifier.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/events/ozone/events_ozone.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/scoped_clipboard_history_pause.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/resources/vector_icons/vector_icons.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/policy/dlp/dlp_browser_helper_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace policy {

namespace {

ui::DataTransferEndpoint CloneEndpoint(
    base::optional_ref<const ui::DataTransferEndpoint> data_endpoint) {
  if (!data_endpoint.has_value()) {
    return ui::DataTransferEndpoint(ui::EndpointType::kDefault);
  }

  return ui::DataTransferEndpoint(*data_endpoint);
}

void SynthesizePaste() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetDisplayForNewWindows().id());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* host = dlp::GetActiveWindowTreeHost();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  DCHECK(host);

  ui::KeyEvent control_press(/*type=*/ui::EventType::kKeyPressed,
                             ui::VKEY_CONTROL,
                             /*code=*/static_cast<ui::DomCode>(0),
                             /*flags=*/0);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Set a property as if this is a key event not consumed by IME.
  // Ozone/wayland IME relies on this flag to work properly.
  ui::SetKeyboardImeFlags(&control_press, ui::kPropertyKeyboardImeIgnoredFlag);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  host->DeliverEventToSink(&control_press);

  ui::KeyEvent v_press(/*type=*/ui::EventType::kKeyPressed, ui::VKEY_V,
                       /*code=*/static_cast<ui::DomCode>(0),
                       /*flags=*/ui::EF_CONTROL_DOWN);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Set a property as if this is a key event not consumed by IME.
  // Ozone/wayland IME relies on this flag to work properly.
  ui::SetKeyboardImeFlags(&v_press, ui::kPropertyKeyboardImeIgnoredFlag);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  host->DeliverEventToSink(&v_press);

  ui::KeyEvent v_release(/*type=*/ui::EventType::kKeyReleased, ui::VKEY_V,
                         /*code=*/static_cast<ui::DomCode>(0),
                         /*flags=*/ui::EF_CONTROL_DOWN);
  host->DeliverEventToSink(&v_release);

  ui::KeyEvent control_release(/*type=*/ui::EventType::kKeyReleased,
                               ui::VKEY_CONTROL,
                               /*code=*/static_cast<ui::DomCode>(0),
                               /*flags=*/0);
  host->DeliverEventToSink(&control_release);
}

bool HasEndpoint(const std::vector<ui::DataTransferEndpoint>& saved_endpoints,
                 base::optional_ref<const ui::DataTransferEndpoint> endpoint) {
  const ui::EndpointType endpoint_type =
      endpoint.has_value() ? endpoint->type() : ui::EndpointType::kDefault;

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
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  DCHECK(data_src.has_value());
  DCHECK(data_src->GetURL());
  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (data_dst.has_value()) {
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
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    base::OnceCallback<void()> reporting_cb) {
  DCHECK(data_src.has_value());
  DCHECK(data_src->GetURL());

  CloseWidget(widget_.get(), views::Widget::ClosedReason::kUnspecified);

  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (data_dst.has_value()) {
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

  std::unique_ptr<ui::ClipboardData> warned_clipboard_data;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ui::DataTransferEndpoint dte(ui::EndpointType::kClipboardHistory);
  auto* data_ptr =
      ui::ClipboardNonBacked::GetForCurrentThread()->GetClipboardData(&dte);
  if (data_ptr) {  // dlp_clipboard_notifier_unittests do not set the clipboard
                   // before calling WarnOnPaste.
    warned_clipboard_data = std::make_unique<ui::ClipboardData>(*data_ptr);
  }
#endif

  auto proceed_cb =
      base::BindOnce(&DlpClipboardNotifier::ProceedPressed,
                     base::Unretained(this), std::move(warned_clipboard_data),
                     CloneEndpoint(data_dst), std::move(reporting_cb));
  auto cancel_cb =
      base::BindOnce(&DlpClipboardNotifier::CancelWarningPressed,
                     base::Unretained(this), CloneEndpoint(data_dst));

  ShowWarningBubble(l10n_util::GetStringFUTF16(
                        IDS_POLICY_DLP_CLIPBOARD_WARN_ON_PASTE, host_name),
                    std::move(proceed_cb), std::move(cancel_cb));
}

void DlpClipboardNotifier::WarnOnBlinkPaste(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> paste_cb) {
  DCHECK(data_src.has_value());
  DCHECK(data_src->GetURL());

  CloseWidget(widget_.get(), views::Widget::ClosedReason::kUnspecified);

  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());

  auto proceed_cb =
      base::BindOnce(&DlpClipboardNotifier::BlinkProceedPressed,
                     base::Unretained(this), CloneEndpoint(data_dst));
  auto cancel_cb =
      base::BindOnce(&DlpClipboardNotifier::CancelWarningPressed,
                     base::Unretained(this), CloneEndpoint(data_dst));

  ShowWarningBubble(l10n_util::GetStringFUTF16(
                        IDS_POLICY_DLP_CLIPBOARD_WARN_ON_PASTE, host_name),
                    std::move(proceed_cb), std::move(cancel_cb));
  SetPasteCallback(std::move(paste_cb));
  Observe(web_contents);
}

bool DlpClipboardNotifier::DidUserApproveDst(
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  return HasEndpoint(approved_dsts_, data_dst);
}

bool DlpClipboardNotifier::DidUserCancelDst(
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  return HasEndpoint(cancelled_dsts_, data_dst);
}

void DlpClipboardNotifier::ProceedPressed(
    std::unique_ptr<ui::ClipboardData> data,
    const ui::DataTransferEndpoint& data_dst,
    base::OnceCallback<void()> reporting_cb,
    views::Widget* widget) {
  CloseWidget(widget, views::Widget::ClosedReason::kAcceptButtonClicked);
  approved_dsts_.push_back(data_dst);

  std::move(reporting_cb).Run();

  if (!display::Screen::GetScreen()) {  // Clipboard related elements do not
                                        // exist in unittests.
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Temporarily ignore clipboard events because we are going to replace the
  // system clipboard and this would otherwise trigger a call to
  // `OnClipboardDataChanged` that resets the user warn selection.
  ignore_clipboard_events_ = true;

  // Pause clipboard history since we are temporarily replacing the system
  // clipboard data with a non user-initiated action.
  auto scoped_clipboard_history_pause =
      ash::ClipboardHistoryController::Get()->CreateScopedPause();

  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  CHECK(clipboard);

  std::unique_ptr<ui::ClipboardData> current_clipboard_data =
      clipboard->WriteClipboardData(std::move(data));
#endif

  SynthesizePaste();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Restore the original system clipboard data.
  ui::ClipboardNonBacked::GetForCurrentThread()->WriteClipboardData(
      std::move(current_clipboard_data));

  ignore_clipboard_events_ = false;
#endif
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
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_CLIPBOARD_BLOCK_TOAST_BUTTON),
      /*dismiss_callback=*/base::BindRepeating(&OnToastClicked),
      /*leading_icon=*/ash::kSystemMenuBusinessIcon);
  ash::ToastManager::Get()->Show(std::move(toast));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void DlpClipboardNotifier::OnClipboardDataChanged() {
  if (ignore_clipboard_events_) {
    return;
  }
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
