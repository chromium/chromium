// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_

#include "base/functional/callback_forward.h"

namespace system_permission_settings {

class MediaAuthorizationWrapper;

// System permission state. These are also used in stats - do not remove or
// re-arrange the values.
enum class SystemPermission {
  kNotDetermined = 0,
  kRestricted = 1,
  kDenied = 2,
  kAllowed = 3,
  kMaxValue = kAllowed
};

// Returns the system permission to capture audio or video.
SystemPermission CheckSystemAudioCapturePermission();
SystemPermission CheckSystemVideoCapturePermission();

// Returns the system permission to capture the screen.
SystemPermission CheckSystemScreenCapturePermission();

// Requests the system permission to capture audio or video. This call
// immediately returns. When requesting permission, the OS will show a user
// dialog and respond asynchronously. At the response, |callback| is posted as a
// reply on the requesting thread.
void RequestSystemAudioCapturePermission(base::OnceClosure callback);
void RequestSystemVideoCapturePermission(base::OnceClosure callback);

// Sets the wrapper object for OS calls. For test mocking purposes.
void SetMediaAuthorizationWrapperForTesting(MediaAuthorizationWrapper* wrapper);

}  // namespace system_permission_settings

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_
