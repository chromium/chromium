// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_WIFI_CREDENTIALS_ATTACHMENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_WIFI_CREDENTIALS_ATTACHMENT_H_

#include <string>

#include "chrome/browser/nearby_sharing/attachment.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"

class WifiCredentialsAttachment : public Attachment {
 public:
  using SecurityType = ::sharing::mojom::WifiCredentialsMetadata::SecurityType;

  WifiCredentialsAttachment(int64_t id,
                            SecurityType security_type,
                            std::string ssid);
  WifiCredentialsAttachment(const WifiCredentialsAttachment&);
  WifiCredentialsAttachment(WifiCredentialsAttachment&&);
  WifiCredentialsAttachment& operator=(const WifiCredentialsAttachment&);
  WifiCredentialsAttachment& operator=(WifiCredentialsAttachment&&);
  ~WifiCredentialsAttachment() override;

  // Attachment:
  void MoveToShareTarget(ShareTarget& share_target) override;
  const std::string& GetDescription() const override;
  nearby_share::mojom::ShareType GetShareType() const override;

  SecurityType security_type() const { return security_type_; }
  const std::string& ssid() const { return ssid_; }
  const std::string& wifi_password() const { return wifi_password_; }
  void set_wifi_password(const std::string& wifi_password) {
    wifi_password_ = wifi_password;
  }

 private:
  SecurityType security_type_;
  std::string ssid_;
  std::string wifi_password_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_WIFI_CREDENTIALS_ATTACHMENT_H_
