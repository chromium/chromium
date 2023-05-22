// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_network_access/private_network_device_chooser_controller.h"

#include <stddef.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_chooser.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::RenderFrameHost;
using content::WebContents;

PrivateNetworkDeviceChooserController::PrivateNetworkDeviceChooserController(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<blink::mojom::PrivateNetworkDevice> device,
    const ChromePrivateNetworkDeviceChooser::EventHandler& event_handler)
    : ChooserController(
          CreateChooserTitle(render_frame_host,
                             IDS_PRIVATE_NETWORK_DEVICE_CHOOSER_PROMPT_ORIGIN)),
      device_(std::move(device)),
      event_handler_(std::move(event_handler)) {
  RenderFrameHost* main_frame = render_frame_host->GetMainFrame();
  origin_ = main_frame->GetLastCommittedOrigin();
}

PrivateNetworkDeviceChooserController::
    ~PrivateNetworkDeviceChooserController() = default;

std::u16string PrivateNetworkDeviceChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_PRIVATE_NETWORK_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT);
}

std::u16string PrivateNetworkDeviceChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::pair<std::u16string, std::u16string>
PrivateNetworkDeviceChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_DEVICE_PERMISSIONS_DIALOG_LOADING_LABEL),
      l10n_util::GetStringUTF16(
          IDS_DEVICE_PERMISSIONS_DIALOG_LOADING_LABEL_TOOLTIP)};
}
size_t PrivateNetworkDeviceChooserController::NumOptions() const {
  return device_ ? 1 : 0;
}

std::u16string PrivateNetworkDeviceChooserController::GetOption(
    size_t index) const {
  // PNA permission prompt only allows one device at once.
  DCHECK(index == 0);
  return l10n_util::GetStringFUTF16(IDS_DEVICE_CHOOSER_DEVICE_NAME_WITH_ID,
                                    base::UTF8ToUTF16(device_->name),
                                    base::UTF8ToUTF16(device_->id));
}

void PrivateNetworkDeviceChooserController::Select(
    const std::vector<size_t>& indices) {}

void PrivateNetworkDeviceChooserController::ReplaceDeviceForTesting(
    std::unique_ptr<blink::mojom::PrivateNetworkDevice> device) {
  device_ = std::move(device);
  if (view()) {
    view()->OnOptionAdded(0);
  }
}

void PrivateNetworkDeviceChooserController::OpenHelpCenterUrl() const {}

void PrivateNetworkDeviceChooserController::Cancel() {}

void PrivateNetworkDeviceChooserController::Close() {}
