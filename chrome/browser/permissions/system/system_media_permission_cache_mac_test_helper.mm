// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_media_permission_cache_mac_test_helper.h"

#import <AVFoundation/AVFoundation.h>

#include "base/functional/callback.h"
#include "chrome/browser/permissions/system/media_authorization_wrapper_mac.h"
#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"

namespace system_permission_settings {

class SystemMediaPermissionCacheMacTestHelper::Impl
    : public MediaAuthorizationWrapper {
 public:
  Impl() {
    SetMediaAuthorizationWrapperForTesting(this);
    SetIsScreenCaptureAllowedForTesting(true);
  }

  ~Impl() override { SetMediaAuthorizationWrapperForTesting(nullptr); }

  AVAuthorizationStatus AuthorizationStatusForMediaType(
      AVMediaType media_type) override {
    if ([media_type isEqualToString:AVMediaTypeVideo]) {
      return camera_status_;
    } else if ([media_type isEqualToString:AVMediaTypeAudio]) {
      return mic_status_;
    }
    return AVAuthorizationStatusNotDetermined;
  }

  void RequestAccessForMediaType(AVMediaType media_type,
                                 base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  void SetCameraStatus(bool denied) {
    camera_status_ =
        denied ? AVAuthorizationStatusDenied : AVAuthorizationStatusAuthorized;
  }

  void SetMicStatus(bool denied) {
    mic_status_ =
        denied ? AVAuthorizationStatusDenied : AVAuthorizationStatusAuthorized;
  }

 private:
  AVAuthorizationStatus camera_status_ = AVAuthorizationStatusNotDetermined;
  AVAuthorizationStatus mic_status_ = AVAuthorizationStatusNotDetermined;
};

SystemMediaPermissionCacheMacTestHelper::
    SystemMediaPermissionCacheMacTestHelper()
    : impl_(std::make_unique<Impl>()) {}

SystemMediaPermissionCacheMacTestHelper::
    ~SystemMediaPermissionCacheMacTestHelper() = default;

void SystemMediaPermissionCacheMacTestHelper::SetCameraStatus(bool denied) {
  impl_->SetCameraStatus(denied);
}

void SystemMediaPermissionCacheMacTestHelper::SetMicStatus(bool denied) {
  impl_->SetMicStatus(denied);
}

}  // namespace system_permission_settings
