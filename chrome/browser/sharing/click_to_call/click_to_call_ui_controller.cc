// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/grit/branded_strings.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_dialog.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"

using SharingMessage = components_sharing_message::SharingMessage;

// static
ClickToCallUiController* ClickToCallUiController::GetOrCreateFromWebContents(
    content::WebContents* web_contents) {
  // Use active WebContents if available.
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (browser)
    web_contents = browser->tab_strip_model()->GetActiveWebContents();
  ClickToCallUiController::CreateForWebContents(web_contents);
  return ClickToCallUiController::FromWebContents(web_contents);
}

// static
void ClickToCallUiController::ShowDialog(
    content::WebContents* web_contents,
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document,
    const GURL& url,
    bool hide_default_handler,
    const std::u16string& program_name) {
  auto* controller = GetOrCreateFromWebContents(web_contents);
  controller->phone_url_ = url;
  controller->initiator_document_ = std::move(initiator_document);
  controller->hide_default_handler_ = hide_default_handler;
  controller->default_program_name_ = program_name;
  controller->UpdateAndShowDialog(initiating_origin);
}

ClickToCallUiController::ClickToCallUiController(
    content::WebContents* web_contents)
    : SharingUiController(web_contents),
      content::WebContentsUserData<ClickToCallUiController>(*web_contents) {}

ClickToCallUiController::~ClickToCallUiController() = default;

void ClickToCallUiController::OnDeviceSelected(
    const std::string& phone_number,
    const SharingTargetDeviceInfo& device,
    SharingClickToCallEntryPoint entry_point) {
  LogClickToCallUKM(web_contents(), entry_point,
                    /*has_devices=*/true, /*has_apps=*/false,
                    SharingClickToCallSelection::kDevice);

  SendNumberToDevice(device, phone_number, entry_point);
}

#if BUILDFLAG(IS_CHROMEOS)

void ClickToCallUiController::OnIntentPickerShown(bool has_devices,
                                                  bool has_apps) {
  UpdateIcon();
  OnDialogShown(has_devices, has_apps);
}

void ClickToCallUiController::OnIntentPickerClosed() {
  UpdateIcon();
}

#endif  // BUILDFLAG(IS_CHROMEOS)

void ClickToCallUiController::OnDialogClosed(SharingDialog* dialog) {
  if (ukm_recorder_ && this->dialog() == dialog)
    std::move(ukm_recorder_).Run(SharingClickToCallSelection::kNone);

  SharingUiController::OnDialogClosed(dialog);
}

std::u16string ClickToCallUiController::GetTitle(
    SharingDialogType dialog_type) {
  switch (dialog_type) {
    case SharingDialogType::kErrorDialog:
      return SharingUiController::GetTitle(dialog_type);
    case SharingDialogType::kEducationalDialog:
      return l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_NO_DEVICES);
    case SharingDialogType::kDialogWithoutDevicesWithApp:
    case SharingDialogType::kDialogWithDevicesMaybeApps:
      return l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
  }
}

PageActionIconType ClickToCallUiController::GetIconType() {
  return PageActionIconType::kClickToCall;
}

sync_pb::SharingSpecificFields::EnabledFeatures
ClickToCallUiController::GetRequiredFeature() const {
  return sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2;
}

void ClickToCallUiController::DoUpdateApps(UpdateAppsCallback callback) {
  std::vector<SharingApp> apps;
  if (hide_default_handler_) {
    std::move(callback).Run(std::move(apps));
    return;
  }

  if (!default_program_name_.empty()) {
    apps.emplace_back(&kOpenInNewIcon, gfx::Image(), default_program_name_,
                      std::string());
  }
  std::move(callback).Run(std::move(apps));
}

void ClickToCallUiController::OnDeviceChosen(
    const SharingTargetDeviceInfo& device) {
  if (ukm_recorder_)
    std::move(ukm_recorder_).Run(SharingClickToCallSelection::kDevice);

  SendNumberToDevice(device, phone_url_.GetContent(),
                     SharingClickToCallEntryPoint::kLeftClickLink);
}

void ClickToCallUiController::SendNumberToDevice(
    const SharingTargetDeviceInfo& device,
    const std::string& phone_number,
    SharingClickToCallEntryPoint entry_point) {
  SharingMessage sharing_message;
  sharing_message.mutable_click_to_call_message()->set_phone_number(
      phone_number);

  SendMessageToDevice(device, /*response_timeout=*/std::nullopt,
                      std::move(sharing_message),
                      /*callback=*/std::nullopt);
}

void ClickToCallUiController::OnAppChosen(const SharingApp& app) {
  if (ukm_recorder_)
    std::move(ukm_recorder_).Run(SharingClickToCallSelection::kApp);

  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
      phone_url_, web_contents(), initiator_document_);
}

void ClickToCallUiController::OnDialogShown(bool has_devices, bool has_apps) {
  if (!HasSendFailed()) {
    // Only left clicks open a dialog.
    ukm_recorder_ = base::BindOnce(&LogClickToCallUKM, web_contents(),
                                   SharingClickToCallEntryPoint::kLeftClickLink,
                                   has_devices, has_apps);
  }
  SharingUiController::OnDialogShown(has_devices, has_apps);
}

std::u16string ClickToCallUiController::GetContentType() const {
  return l10n_util::GetStringUTF16(IDS_BROWSER_SHARING_CONTENT_TYPE_NUMBER);
}

const gfx::VectorIcon& ClickToCallUiController::GetVectorIcon() const {
  return vector_icons::kCallRefreshIcon;
}

std::u16string ClickToCallUiController::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
}

SharingFeatureName ClickToCallUiController::GetFeatureMetricsPrefix() const {
  return SharingFeatureName::kClickToCall;
}

SharingDialogData ClickToCallUiController::CreateDialogData(
    SharingDialogType dialog_type) {
  SharingDialogData data = SharingUiController::CreateDialogData(dialog_type);

  // Do not add the header image for error dialogs.
  if (dialog_type != SharingDialogType::kErrorDialog) {
    data.header_icons = SharingDialogData::HeaderIcons(
        &kClickToCallIllustrationIcon, &kClickToCallIllustrationDarkIcon);
  }

  data.help_text_id =
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES;
  data.help_text_origin_id =
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES_ORIGIN;
  data.origin_text_id =
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_INITIATING_ORIGIN;

  return data;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClickToCallUiController);
