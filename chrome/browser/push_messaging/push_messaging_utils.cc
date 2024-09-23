// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_utils.h"

#include "base/base64url.h"
#include "base/version_info/channel.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "chrome/common/channel_info.h"
#include "url/gurl.h"

namespace push_messaging {

std::string GetGcmEndpointForChannel(version_info::Channel channel) {
  if (base::FeatureList::IsEnabled(
          features::kPushMessagingGcmEndpointEnvironment)) {
    if (channel != version_info::Channel::STABLE) {
      return kPushMessagingStagingGcmEndpoint;
    }
  }
  return kPushMessagingGcmEndpoint;
}

GURL CreateEndpoint(const std::string& subscription_id) {
  const GURL endpoint(GetGcmEndpointForChannel(chrome::GetChannel()) +
                      subscription_id);
  DCHECK(endpoint.is_valid());
  return endpoint;
}

blink::mojom::PushSubscriptionOptionsPtr MakeOptions(
    const std::string& sender_id) {
  return blink::mojom::PushSubscriptionOptions::New(
      /*user_visible_only=*/true,
      std::vector<uint8_t>(sender_id.begin(), sender_id.end()));
}

bool IsVapidKey(const std::string& application_server_key) {
  // VAPID keys are NIST P-256 public keys in uncompressed format (64 bytes),
  // verified through its length and the 0x04 prefix.
  return application_server_key.size() == 65 &&
         application_server_key[0] == 0x04;
}

std::string NormalizeSenderInfo(const std::string& application_server_key) {
  if (!IsVapidKey(application_server_key))
    return application_server_key;

  std::string encoded_application_server_key;
  base::Base64UrlEncode(application_server_key,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_application_server_key);

  return encoded_application_server_key;
}

}  // namespace push_messaging
