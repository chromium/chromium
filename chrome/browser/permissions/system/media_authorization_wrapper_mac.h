// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_MEDIA_AUTHORIZATION_WRAPPER_MAC_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_MEDIA_AUTHORIZATION_WRAPPER_MAC_H_

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "base/functional/callback_forward.h"

namespace system_permission_settings {

class MediaAuthorizationWrapper {
 public:
  virtual ~MediaAuthorizationWrapper() {}

  virtual AVAuthorizationStatus AuthorizationStatusForMediaType(
      NSString* media_type) = 0;
  virtual void RequestAccessForMediaType(NSString* media_type,
                                         base::OnceClosure callback) = 0;
};

}  // namespace system_permission_settings

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_MEDIA_AUTHORIZATION_WRAPPER_MAC_H_
