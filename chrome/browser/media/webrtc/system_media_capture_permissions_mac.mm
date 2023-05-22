// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

#import <AVFoundation/AVFoundation.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/webrtc/media_authorization_wrapper_mac.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#include "chrome/common/chrome_features.h"
#include "media/base/media_switches.h"
#include "ui/base/cocoa/permissions_utils.h"

namespace system_media_permissions {

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

  NSInteger AuthorizationStatusForMediaType(AVMediaType media_type) override {
    if (@available(macOS 10.14, *)) {
      return [AVCaptureDevice authorizationStatusForMediaType:media_type];
    } else {
      CHECK(false);
      return 0;
    }
  }

  void RequestAccessForMediaType(AVMediaType media_type,
                                 base::OnceClosure callback) override {
    if (@available(macOS 10.14, *)) {
      __block base::OnceClosure block_callback = std::move(callback);
      __block scoped_refptr<base::SequencedTaskRunner> requesting_thread =
          base::SequencedTaskRunner::GetCurrentDefault();
      [AVCaptureDevice requestAccessForMediaType:media_type
                               completionHandler:^(BOOL granted) {
                                 requesting_thread->PostTask(
                                     FROM_HERE, std::move(block_callback));
                               }];
    } else {
      CHECK(false);
    }
  }
};

MediaAuthorizationWrapper& GetMediaAuthorizationWrapper() {
  if (g_media_authorization_wrapper_for_tests)
    return *g_media_authorization_wrapper_for_tests;

  static base::NoDestructor<MediaAuthorizationWrapperImpl>
      media_authorization_wrapper;
  return *media_authorization_wrapper;
}

NSInteger MediaAuthorizationStatus(AVMediaType media_type) {
  if (@available(macOS 10.14, *)) {
    return GetMediaAuthorizationWrapper().AuthorizationStatusForMediaType(
        media_type);
  }

  CHECK(false);
  return 0;
}

SystemPermission CheckSystemMediaCapturePermission(AVMediaType media_type) {
  if (UsingFakeMediaDevices())
    return SystemPermission::kAllowed;

  if (@available(macOS 10.14, *)) {
    NSInteger auth_status = MediaAuthorizationStatus(media_type);
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
        NOTREACHED_NORETURN();
    }
  }

  // On pre-10.14, there are no system permissions, so we return allowed.
  return SystemPermission::kAllowed;
}

void RequestSystemMediaCapturePermission(AVMediaType media_type,
                                         base::OnceClosure callback) {
  if (UsingFakeMediaDevices()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  if (@available(macOS 10.14, *)) {
    GetMediaAuthorizationWrapper().RequestAccessForMediaType(
        media_type, std::move(callback));
  } else {
    CHECK(false);
  }
}

bool IsScreenCaptureAllowed() {
  if (@available(macOS 10.15, *)) {
    if (!base::FeatureList::IsEnabled(
            features::kMacSystemScreenCapturePermissionCheck)) {
      return true;
    }
  }

  bool allowed = ui::IsScreenCaptureAllowed();
  LogSystemScreenCapturePermission(allowed);
  return allowed;
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

void RequestSystemAudioCapturePermisson(base::OnceClosure callback) {
  RequestSystemMediaCapturePermission(AVMediaTypeAudio, std::move(callback));
}

void RequestSystemVideoCapturePermisson(base::OnceClosure callback) {
  RequestSystemMediaCapturePermission(AVMediaTypeVideo, std::move(callback));
}

void SetMediaAuthorizationWrapperForTesting(
    MediaAuthorizationWrapper* wrapper) {
  CHECK(!g_media_authorization_wrapper_for_tests);
  g_media_authorization_wrapper_for_tests = wrapper;
}

}  // namespace system_media_permissions
