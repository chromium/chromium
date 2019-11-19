// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_AUTHORIZATION_WRAPPER_MAC_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_AUTHORIZATION_WRAPPER_MAC_H_

#import <Foundation/NSString.h>

#include "base/callback_forward.h"

namespace system_media_permissions {

class MediaAuthorizationWrapper {
 public:
  virtual ~MediaAuthorizationWrapper() {}

  virtual NSInteger AuthorizationStatusForMediaType(NSString* media_type) = 0;
  virtual void RequestAccessForMediaType(NSString* media_type,
                                         base::RepeatingClosure callback,
                                         const base::TaskTraits& traits) = 0;
};

}  // namespace system_media_permissions

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_AUTHORIZATION_WRAPPER_MAC_H_
