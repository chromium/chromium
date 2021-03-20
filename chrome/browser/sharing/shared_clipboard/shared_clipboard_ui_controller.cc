// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_ui_controller.h"

#include <utility>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

// static
SharedClipboardUiController*
SharedClipboardUiController::GetOrCreateFromWebContents(
    content::WebContents* web_contents) {
  // Use active WebContents if available.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser)
    web_contents = browser->tab_strip_model()->GetActiveWebContents();
  SharedClipboardUiController::CreateForWebContents(web_contents);
  return SharedClipboardUiController::FromWebContents(web_contents);
}

SharedClipboardUiController::SharedClipboardUiController(
    content::WebContents* web_contents)
    : SharingUiController(web_contents) {}

SharedClipboardUiController::~SharedClipboardUiController() = default;

void SharedClipboardUiController::OnDeviceSelected(
    const std::u16string& text,
    const syncer::DeviceInfo& device) {
  text_ = text;
  OnDeviceChosen(device);
}

std::u16string SharedClipboardUiController::GetTitle(
    SharingDialogType dialog_type) {
  // Shared clipboard only shows error dialogs.
  DCHECK_EQ(SharingDialogType::kErrorDialog, dialog_type);

  if (send_result() == SharingSendMessageResult::kPayloadTooLarge) {
    return l10n_util::GetStringUTF16(
        IDS_BROWSER_SHARING_SHARED_CLIPBOARD_ERROR_DIALOG_TITLE_PAYLOAD_TOO_LARGE);
  }

  return SharingUiController::GetTitle(dialog_type);
}

PageActionIconType SharedClipboardUiController::GetIconType() {
  return PageActionIconType::kSharedClipboard;
}

sync_pb::SharingSpecificFields::EnabledFeatures
SharedClipboardUiController::GetRequiredFeature() const {
  return sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2;
}

// No need for apps for shared clipboard feature
void SharedClipboardUiController::DoUpdateApps(UpdateAppsCallback callback) {
  std::move(callback).Run(std::vector<SharingApp>());
}

void SharedClipboardUiController::OnDeviceChosen(
    const syncer::DeviceInfo& device) {
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_shared_clipboard_message()->set_text(
      base::UTF16ToUTF8(text_));

  SendMessageToDevice(device, /*response_timeout=*/base::nullopt,
                      std::move(sharing_message),
                      /*callback=*/base::nullopt);
}

void SharedClipboardUiController::OnAppChosen(const SharingApp& app) {
  // Do nothing - there is no apps
}

std::u16string SharedClipboardUiController::GetContentType() const {
  return l10n_util::GetStringUTF16(IDS_BROWSER_SHARING_CONTENT_TYPE_TEXT);
}

std::u16string SharedClipboardUiController::GetErrorDialogText() const {
  if (send_result() == SharingSendMessageResult::kPayloadTooLarge) {
    return l10n_util::GetStringUTF16(
        IDS_BROWSER_SHARING_SHARED_CLIPBOARD_ERROR_DIALOG_TEXT_PAYLOAD_TOO_LARGE);
  }

  return SharingUiController::GetErrorDialogText();
}

const gfx::VectorIcon& SharedClipboardUiController::GetVectorIcon() const {
  return kCopyIcon;
}

std::u16string SharedClipboardUiController::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_OMNIBOX_TOOLTIP_SHARED_CLIPBOARD);
}

SharingFeatureName SharedClipboardUiController::GetFeatureMetricsPrefix()
    const {
  return SharingFeatureName::kSharedClipboard;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharedClipboardUiController)
