// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"

#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

namespace {
bool command_line_enabled_for_testing = false;
}  // namespace

namespace media_router {

bool IsCommandLineSwitchSupported() {
  if (command_line_enabled_for_testing)
    return true;
  version_info::Channel channel = chrome::GetChannel();
  return channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA;
}

void EnableCommandLineSupportForTesting() {
  command_line_enabled_for_testing = true;
}

constexpr char kGetMethod[] = "GET";
constexpr char kContentType[] = "application/json; charset=UTF-8";
constexpr char kDiscoveryOAuth2Scope[] =
    "https://www.googleapis.com/auth/cast-edu-messaging";

constexpr char kDefaultDiscoveryEndpoint[] =
    "https://castedumessaging-pa.googleapis.com";

constexpr char kDiscoveryServicePath[] = "/v1/receivers";
constexpr char kDiscoveryOAuthConsumerName[] = "access_code_cast_discovery";
constexpr char kEmptyPostData[] = "";

constexpr char kJsonDevice[] = "device";
constexpr char kJsonDisplayName[] = "displayName";
constexpr char kJsonId[] = "id";

constexpr char kJsonNetworkInfo[] = "networkInfo";
constexpr char kJsonHostName[] = "hostName";
constexpr char kJsonPort[] = "port";
constexpr char kJsonIpV4Address[] = "ipV4Address";
constexpr char kJsonIpV6Address[] = "ipV6Address";

constexpr char kJsonDeviceCapabilities[] = "deviceCapabilities";
constexpr char kJsonVideoOut[] = "videoOut";
constexpr char kJsonVideoIn[] = "videoIn";
constexpr char kJsonAudioOut[] = "audioOut";
constexpr char kJsonAudioIn[] = "audioIn";
constexpr char kJsonDevMode[] = "devMode";

constexpr char kJsonError[] = "error";
constexpr char kJsonErrorCode[] = "code";
constexpr char kJsonErrorMessage[] = "message";

constexpr char kAccessCodeCastFlagParamType[] = "access_code_cast_duration_s";
namespace switches {
constexpr char kDiscoveryEndpointSwitch[] = "access-code-cast-url";

}  // namespace switches
}  // namespace media_router
