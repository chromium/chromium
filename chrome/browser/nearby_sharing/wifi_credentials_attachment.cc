// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/wifi_credentials_attachment.h"

#include <utility>

#include "chrome/browser/nearby_sharing/share_target.h"

WifiCredentialsAttachment::WifiCredentialsAttachment(int64_t id,
                                                     SecurityType security_type,
                                                     std::string ssid)
    : Attachment(id, Attachment::Family::kWifiCredentials, /*size=*/0),
      security_type_(security_type),
      ssid_(std::move(ssid)) {}

WifiCredentialsAttachment::WifiCredentialsAttachment(
    const WifiCredentialsAttachment&) = default;

WifiCredentialsAttachment::WifiCredentialsAttachment(
    WifiCredentialsAttachment&&) = default;

WifiCredentialsAttachment& WifiCredentialsAttachment::operator=(
    const WifiCredentialsAttachment&) = default;

WifiCredentialsAttachment& WifiCredentialsAttachment::operator=(
    WifiCredentialsAttachment&&) = default;

WifiCredentialsAttachment::~WifiCredentialsAttachment() = default;

void WifiCredentialsAttachment::MoveToShareTarget(ShareTarget& share_target) {
  share_target.wifi_credentials_attachments.push_back(std::move(*this));
}

const std::string& WifiCredentialsAttachment::GetDescription() const {
  return ssid_;
}

nearby_share::mojom::ShareType WifiCredentialsAttachment::GetShareType() const {
  return nearby_share::mojom::ShareType::kWifiCredentials;
}
