// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_ui_controller.h"

#include <utility>

#include "base/time/time.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_dialog.h"
#include "components/sharing_message/sharing_dialog_data.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

BrowserWindow* GetWindowFromWebContents(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  return browser ? browser->window() : nullptr;
}

content::WebContents* GetCurrentWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  return browser ? browser->tab_strip_model()->GetActiveWebContents() : nullptr;
}

SharingDialogType GetSharingDialogType(bool has_devices, bool has_apps) {
  if (has_devices)
    return SharingDialogType::kDialogWithDevicesMaybeApps;
  if (has_apps)
    return SharingDialogType::kDialogWithoutDevicesWithApp;
  return SharingDialogType::kEducationalDialog;
}

}  // namespace

SharingUiController::SharingUiController(content::WebContents* web_contents)
    : web_contents_(web_contents),
      sharing_service_(SharingServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {}

SharingUiController::~SharingUiController() = default;

std::u16string SharingUiController::GetTitle(SharingDialogType dialog_type) {
  // We only handle error messages generically.
  DCHECK_EQ(SharingDialogType::kErrorDialog, dialog_type);
  switch (send_result()) {
    case SharingSendMessageResult::kDeviceNotFound:
    case SharingSendMessageResult::kNetworkError:
    case SharingSendMessageResult::kAckTimeout:
    case SharingSendMessageResult::kCommitTimeout:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TITLE_GENERIC_ERROR,
          GetContentType());

    case SharingSendMessageResult::kSuccessful:
    case SharingSendMessageResult::kCancelled:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];

    case SharingSendMessageResult::kPayloadTooLarge:
    case SharingSendMessageResult::kInternalError:
    case SharingSendMessageResult::kEncryptionError:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TITLE_INTERNAL_ERROR,
          GetContentType());
  }
}

std::u16string SharingUiController::GetErrorDialogText() const {
  switch (send_result()) {
    case SharingSendMessageResult::kDeviceNotFound:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_DEVICE_NOT_FOUND,
          GetTargetDeviceName());

    case SharingSendMessageResult::kCommitTimeout:
    case SharingSendMessageResult::kNetworkError:
      return l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_NETWORK_ERROR);

    case SharingSendMessageResult::kAckTimeout:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_DEVICE_ACK_TIMEOUT,
          GetTargetDeviceName());

    case SharingSendMessageResult::kSuccessful:
    case SharingSendMessageResult::kCancelled:
      return std::u16string();

    case SharingSendMessageResult::kPayloadTooLarge:
    case SharingSendMessageResult::kInternalError:
    case SharingSendMessageResult::kEncryptionError:
      return l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_INTERNAL_ERROR);
  }
}

void SharingUiController::OnDialogClosed(SharingDialog* dialog) {
  // Ignore already replaced dialogs.
  if (dialog != dialog_)
    return;

  dialog_ = nullptr;
  UpdateIcon();
}

void SharingUiController::OnDialogShown(bool has_devices, bool has_apps) {
  if (on_dialog_shown_closure_for_testing_)
    std::move(on_dialog_shown_closure_for_testing_).Run();
}

void SharingUiController::ClearLastDialog() {
  last_dialog_id_++;
  is_loading_ = false;
  send_result_ = SharingSendMessageResult::kSuccessful;
  CloseDialog();
}

void SharingUiController::UpdateAndShowDialog(
    const std::optional<url::Origin>& initiating_origin) {
  ClearLastDialog();
  DoUpdateApps(base::BindOnce(&SharingUiController::OnAppsReceived,
                              weak_ptr_factory_.GetWeakPtr(), last_dialog_id_,
                              initiating_origin));
}

std::vector<SharingTargetDeviceInfo> SharingUiController::GetDevices() const {
  return sharing_service_->GetDeviceCandidates(GetRequiredFeature());
}

bool SharingUiController::HasSendFailed() const {
  return send_result_ != SharingSendMessageResult::kSuccessful &&
         send_result_ != SharingSendMessageResult::kCancelled;
}

void SharingUiController::MaybeShowErrorDialog() {
  if (HasSendFailed() && web_contents_ == GetCurrentWebContents(web_contents_))
    ShowNewDialog(CreateDialogData(SharingDialogType::kErrorDialog));
}

SharingDialogData SharingUiController::CreateDialogData(
    SharingDialogType dialog_type) {
  SharingDialogData data;

  data.type = dialog_type;
  data.prefix = GetFeatureMetricsPrefix();
  data.title = GetTitle(data.type);
  data.error_text = GetErrorDialogText();

  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  data.device_callback =
      base::BindOnce(&SharingUiController::OnDeviceChosen, weak_ptr);
  data.app_callback =
      base::BindOnce(&SharingUiController::OnAppChosen, weak_ptr);
  data.close_callback =
      base::BindOnce(&SharingUiController::OnDialogClosed, weak_ptr);
  return data;
}

bool SharingUiController::ShouldShowLoadingIcon() const {
  return true;
}

bool SharingUiController::HasAccessibleUi() const {
  return true;
}

base::OnceClosure SharingUiController::SendMessageToDevice(
    const SharingTargetDeviceInfo& device,
    std::optional<base::TimeDelta> response_timeout,
    components_sharing_message::SharingMessage sharing_message,
    std::optional<SharingMessageSender::ResponseCallback> custom_callback) {
  send_result_ = SharingSendMessageResult::kSuccessful;
  target_device_name_ = device.client_name();
  if (ShouldShowLoadingIcon()) {
    last_dialog_id_++;
    is_loading_ = true;
    UpdateIcon();
  }

  SharingMessageSender::ResponseCallback response_callback = base::BindOnce(
      &SharingUiController::OnResponse, weak_ptr_factory_.GetWeakPtr(),
      last_dialog_id_, std::move(custom_callback));
  return sharing_service_->SendMessageToDevice(
      device, response_timeout.value_or(kSharingMessageTTL),
      std::move(sharing_message), std::move(response_callback));
}

void SharingUiController::UpdateIcon() {
  BrowserWindow* window = GetWindowFromWebContents(web_contents_);
  if (!window)
    return;

  window->UpdatePageActionIcon(GetIconType());
}

void SharingUiController::CloseDialog() {
  if (!dialog_)
    return;

  dialog_->Hide();

  // SharingDialog::Hide may close the dialog asynchronously, and therefore not
  // call OnDialogClosed immediately. If that is the case, call OnDialogClosed
  // now to notify subclasses and clear |dialog_|.
  if (dialog_)
    OnDialogClosed(dialog_);

  DCHECK(!dialog_);
}

void SharingUiController::ShowNewDialog(SharingDialogData dialog_data) {
  CloseDialog();
  BrowserWindow* window = GetWindowFromWebContents(web_contents_);
  if (!window)
    return;
  bool has_devices = !dialog_data.devices.empty();
  bool has_apps = !dialog_data.apps.empty();
  dialog_ = window->ShowSharingDialog(web_contents(), std::move(dialog_data));
  UpdateIcon();
  OnDialogShown(has_devices, has_apps);
}

std::u16string SharingUiController::GetTargetDeviceName() const {
  return base::UTF8ToUTF16(target_device_name_);
}

void SharingUiController::OnResponse(
    int dialog_id,
    std::optional<SharingMessageSender::ResponseCallback> custom_callback,
    SharingSendMessageResult result,
    std::unique_ptr<components_sharing_message::ResponseMessage> response) {
  if (custom_callback)
    std::move(custom_callback.value()).Run(result, std::move(response));
  if (dialog_id != last_dialog_id_)
    return;

  send_result_ = result;
  if (ShouldShowLoadingIcon()) {
    is_loading_ = false;
    UpdateIcon();
  }
}

void SharingUiController::OnAppsReceived(
    int dialog_id,
    const std::optional<url::Origin>& initiating_origin,
    std::vector<SharingApp> apps) {
  if (dialog_id != last_dialog_id_)
    return;

  auto devices = GetDevices();

  SharingDialogData dialog_data =
      CreateDialogData(GetSharingDialogType(!devices.empty(), !apps.empty()));
  dialog_data.devices = std::move(devices);
  dialog_data.apps = std::move(apps);
  dialog_data.initiating_origin = initiating_origin;

  ShowNewDialog(std::move(dialog_data));
}
