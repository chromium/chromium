// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"

#import <AVFoundation/AVFoundation.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/permissions/system/media_authorization_wrapper_mac.h"
#include "chrome/common/chrome_features.h"
#include "media/base/media_switches.h"
#include "ui/base/cocoa/permissions_utils.h"

namespace system_permission_settings {

namespace {

bool UsingFakeMediaDevices() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream);
}

// Pointer to OS call wrapper that tests can set.
MediaAuthorizationWrapper* g_media_authorization_wrapper_for_tests = nullptr;

// Implementation of OS call wrapper that does the actual OS calls.
class MediaAuthorizationWrapperImpl final : public MediaAuthorizationWrapper {
 public:
  MediaAuthorizationWrapperImpl() = default;

  MediaAuthorizationWrapperImpl(const MediaAuthorizationWrapperImpl&) = delete;
  MediaAuthorizationWrapperImpl& operator=(
      const MediaAuthorizationWrapperImpl&) = delete;

  ~MediaAuthorizationWrapperImpl() override = default;

  AVAuthorizationStatus AuthorizationStatusForMediaType(
      AVMediaType media_type) override {
    return [AVCaptureDevice authorizationStatusForMediaType:media_type];
  }

  void RequestAccessForMediaType(AVMediaType media_type,
                                 base::OnceClosure callback) override {
    __block base::OnceClosure block_callback = std::move(callback);
    __block scoped_refptr<base::SequencedTaskRunner> requesting_thread =
        base::SequencedTaskRunner::GetCurrentDefault();
    [AVCaptureDevice requestAccessForMediaType:media_type
                             completionHandler:^(BOOL granted) {
                               requesting_thread->PostTask(
                                   FROM_HERE, std::move(block_callback));
                             }];
  }
};

MediaAuthorizationWrapper& GetMediaAuthorizationWrapper() {
  if (g_media_authorization_wrapper_for_tests) {
    return *g_media_authorization_wrapper_for_tests;
  }

  static base::NoDestructor<MediaAuthorizationWrapperImpl>
      media_authorization_wrapper;
  return *media_authorization_wrapper;
}

AVAuthorizationStatus MediaAuthorizationStatus(AVMediaType media_type) {
  return GetMediaAuthorizationWrapper().AuthorizationStatusForMediaType(
      media_type);
}

SystemPermission CheckSystemMediaCapturePermission(AVMediaType media_type) {
  if (UsingFakeMediaDevices()) {
    return SystemPermission::kAllowed;
  }

  AVAuthorizationStatus auth_status = MediaAuthorizationStatus(media_type);
  switch (auth_status) {
    case AVAuthorizationStatusNotDetermined:
      return SystemPermission::kNotDetermined;
    case AVAuthorizationStatusRestricted:
      return SystemPermission::kRestricted;
    case AVAuthorizationStatusDenied:
      return SystemPermission::kDenied;
    case AVAuthorizationStatusAuthorized:
      return SystemPermission::kAllowed;
    default:
      NOTREACHED();
  }
}

void RequestSystemMediaCapturePermission(AVMediaType media_type,
                                         base::OnceClosure callback) {
  if (UsingFakeMediaDevices()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  GetMediaAuthorizationWrapper().RequestAccessForMediaType(media_type,
                                                           std::move(callback));
}

bool IsScreenCaptureAllowed() {
  if (!base::FeatureList::IsEnabled(
          features::kMacSystemScreenCapturePermissionCheck)) {
    return true;
  }

  return ui::IsScreenCaptureAllowed();
}

}  // namespace

SystemPermission CheckSystemAudioCapturePermission() {
  return CheckSystemMediaCapturePermission(AVMediaTypeAudio);
}

SystemPermission CheckSystemVideoCapturePermission() {
  return CheckSystemMediaCapturePermission(AVMediaTypeVideo);
}

SystemPermission CheckSystemScreenCapturePermission() {
  return IsScreenCaptureAllowed() ? SystemPermission::kAllowed
                                  : SystemPermission::kDenied;
}

void RequestSystemAudioCapturePermission(base::OnceClosure callback) {
  RequestSystemMediaCapturePermission(AVMediaTypeAudio, std::move(callback));
}

void RequestSystemVideoCapturePermission(base::OnceClosure callback) {
  RequestSystemMediaCapturePermission(AVMediaTypeVideo, std::move(callback));
}

void SetMediaAuthorizationWrapperForTesting(
    MediaAuthorizationWrapper* wrapper) {
  CHECK(!g_media_authorization_wrapper_for_tests);
  g_media_authorization_wrapper_for_tests = wrapper;
}

}  // namespace system_permission_settings
