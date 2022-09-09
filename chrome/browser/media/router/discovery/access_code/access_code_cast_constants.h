// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_CONSTANTS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_CONSTANTS_H_

namespace media_router {
bool IsCommandLineSwitchSupported();
void EnableCommandLineSupportForTesting();

extern const char kGetMethod[];
extern const char kContentType[];
extern const char kDiscoveryOAuth2Scope[];

extern const char kDefaultDiscoveryEndpoint[];

extern const char kDiscoveryServicePath[];
extern const char kDiscoveryOAuthConsumerName[];
extern const char kEmptyPostData[];

extern const char kJsonDevice[];
extern const char kJsonDisplayName[];
extern const char kJsonId[];

extern const char kJsonNetworkInfo[];
extern const char kJsonHostName[];
extern const char kJsonPort[];
extern const char kJsonIpV4Address[];
extern const char kJsonIpV6Address[];

extern const char kJsonDeviceCapabilities[];
extern const char kJsonVideoOut[];
extern const char kJsonVideoIn[];
extern const char kJsonAudioOut[];
extern const char kJsonAudioIn[];
extern const char kJsonDevMode[];

extern const char kJsonError[];
extern const char kJsonErrorCode[];
extern const char kJsonErrorMessage[];

extern const char kAccessCodeCastFlagParamType[];
namespace switches {
// Specifies the URL from which to obtain cast discovery information.
extern const char kDiscoveryEndpointSwitch[];

}  // namespace switches
}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_CONSTANTS_H_
