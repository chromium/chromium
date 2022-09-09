// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSIONS_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSIONS_H_

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

class GURL;
class Profile;

enum MediaStreamDevicePolicy {
  POLICY_NOT_SET,
  ALWAYS_DENY,
  ALWAYS_ALLOW,
};

// Get the device policy for |security_origin| and |profile|.
MediaStreamDevicePolicy GetDevicePolicy(const Profile* profile,
                                        const GURL& security_origin,
                                        const char* policy_name,
                                        const char* allowed_urls_pref_name);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSIONS_H_
