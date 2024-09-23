// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/wifi_credentials_attachment.h"

#include "chrome/browser/nearby_sharing/share_target.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestSsid[] = "test_SSID";
const char kTestPassword[] = "T35t_P@ssw0rd";
const WifiCredentialsAttachment::SecurityType kTestSecurityType =
    ::sharing::mojom::WifiCredentialsMetadata::SecurityType::kWpaPsk;

}  // namespace
TEST(WifiCredentialsAttachmentTest, WifiCredentialsAttachmentTest) {
  WifiCredentialsAttachment attachment(/*id=*/0, kTestSecurityType, kTestSsid);
  attachment.set_wifi_password(kTestPassword);

  EXPECT_EQ(kTestSecurityType, attachment.security_type());
  EXPECT_EQ(kTestSsid, attachment.ssid());
  EXPECT_EQ(kTestPassword, attachment.wifi_password());
  EXPECT_EQ(kTestSsid, attachment.GetDescription());
  EXPECT_EQ(nearby_share::mojom::ShareType::kWifiCredentials,
            attachment.GetShareType());

  ShareTarget share_target;
  attachment.MoveToShareTarget(share_target);
  EXPECT_TRUE(share_target.has_attachments());
  EXPECT_EQ(1u, share_target.wifi_credentials_attachments.size());
}
