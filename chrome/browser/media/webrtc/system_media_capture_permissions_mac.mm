// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Authorization functions and types are available on 10.14+.
// To avoid availability compile errors, use performSelector invocation of
// functions, NSInteger instead of AVAuthorizationStatus, and NSString* instead
// of AVMediaType.
// The AVAuthorizationStatus enum is defined as follows (10.14 SDK):
// AVAuthorizationStatusNotDetermined = 0,
// AVAuthorizationStatusRestricted    = 1,
// AVAuthorizationStatusDenied        = 2,
// AVAuthorizationStatusAuthorized    = 3,
// TODO(grunell): Call functions directly and use AVAuthorizationStatus once
// we use the 10.14 SDK.

#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

#import <AVFoundation/AVFoundation.h>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/media/webrtc/media_authorization_wrapper_mac.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/media_switches.h"

namespace system_media_permissions {

namespace {

bool UsingFakeMediaDevices() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream);
}

// Pointer to OS call wrapper that tests can set.
MediaAuthorizationWrapper* g_media_authorization_wrapper_for_tests = nullptr;

// Implementation of OS call wrapper that does the actual OS calls.
class MediaAuthorizationWrapperImpl : public MediaAuthorizationWrapper {
 public:
  MediaAuthorizationWrapperImpl() = default;
  ~MediaAuthorizationWrapperImpl() final = default;

  NSInteger AuthorizationStatusForMediaType(NSString* media_type) final {
    if (@available(macOS 10.14, *)) {
      Class target = [AVCaptureDevice class];
      SEL selector = @selector(authorizationStatusForMediaType:);
      NSInteger auth_status = 0;
      if ([target respondsToSelector:selector]) {
        auth_status =
            (NSInteger)[target performSelector:selector withObject:media_type];
      } else {
        DLOG(WARNING)
            << "authorizationStatusForMediaType could not be executed";
      }
      return auth_status;
    }

    NOTREACHED();
    return 0;
  }

  void RequestAccessForMediaType(NSString* media_type,
                                 base::RepeatingClosure callback,
                                 const base::TaskTraits& traits) final {
    if (@available(macOS 10.14, *)) {
      Class target = [AVCaptureDevice class];
      SEL selector = @selector(requestAccessForMediaType:completionHandler:);
      if ([target respondsToSelector:selector]) {
        [target performSelector:selector
                     withObject:media_type
                     withObject:^(BOOL granted) {
                       base::PostTask(FROM_HERE, traits, std::move(callback));
                     }];
      } else {
        DLOG(WARNING) << "requestAccessForMediaType could not be executed";
        base::PostTask(FROM_HERE, traits, std::move(callback));
      }
    } else {
      NOTREACHED();
      base::PostTask(FROM_HERE, traits, std::move(callback));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaAuthorizationWrapperImpl);
};

MediaAuthorizationWrapper& GetMediaAuthorizationWrapper() {
  if (g_media_authorization_wrapper_for_tests)
    return *g_media_authorization_wrapper_for_tests;

  static base::NoDestructor<MediaAuthorizationWrapperImpl>
      media_authorization_wrapper;
  return *media_authorization_wrapper;
}

NSInteger MediaAuthorizationStatus(NSString* media_type) {
  if (@available(macOS 10.14, *)) {
    return GetMediaAuthorizationWrapper().AuthorizationStatusForMediaType(
        media_type);
  }

  NOTREACHED();
  return 0;
}

SystemPermission CheckSystemMediaCapturePermission(NSString* media_type) {
  if (UsingFakeMediaDevices())
    return SystemPermission::kAllowed;

  if (@available(macOS 10.14, *)) {
    NSInteger auth_status = MediaAuthorizationStatus(media_type);
    switch (auth_status) {
      case 0:
        return SystemPermission::kNotDetermined;
      case 1:
        return SystemPermission::kRestricted;
      case 2:
        return SystemPermission::kDenied;
      case 3:
        return SystemPermission::kAllowed;
      default:
        NOTREACHED();
        return SystemPermission::kAllowed;
    }
  }

  // On pre-10.14, there are no system permissions, so we return allowed.
  return SystemPermission::kAllowed;
}

// Use RepeatingCallback since it must be copyable for use in the block. It's
// only called once though.
void RequestSystemMediaCapturePermission(NSString* media_type,
                                         base::RepeatingClosure callback,
                                         const base::TaskTraits& traits) {
  if (UsingFakeMediaDevices()) {
    base::PostTask(FROM_HERE, traits, std::move(callback));
    return;
  }

  if (@available(macOS 10.14, *)) {
    GetMediaAuthorizationWrapper().RequestAccessForMediaType(
        media_type, std::move(callback), traits);
  } else {
    NOTREACHED();
    // Should never happen since for pre-10.14 system permissions don't exist
    // and checking them in CheckSystemAudioCapturePermission() will always
    // return allowed, and this function should not be called.
    base::PostTask(FROM_HERE, traits, std::move(callback));
  }
}

// Heuristic to check screen capture permission on macOS 10.15.
// Screen Capture is considered allowed if the name of at least one normal
// or dock window running on another process is visible.
// See https://crbug.com/993692.
bool IsScreenCaptureAllowed() {
  if (@available(macOS 10.15, *)) {
    if (!base::FeatureList::IsEnabled(
            features::kMacSystemScreenCapturePermissionCheck)) {
      return true;
    }

    base::ScopedCFTypeRef<CFArrayRef> window_list(
        CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID));
    int current_pid = [[NSProcessInfo processInfo] processIdentifier];
    for (NSDictionary* window in base::mac::CFToNSCast(window_list.get())) {
      NSNumber* window_pid =
          [window objectForKey:base::mac::CFToNSCast(kCGWindowOwnerPID)];
      if (!window_pid || [window_pid integerValue] == current_pid)
        continue;

      NSString* window_name =
          [window objectForKey:base::mac::CFToNSCast(kCGWindowName)];
      if (!window_name)
        continue;

      NSNumber* layer =
          [window objectForKey:base::mac::CFToNSCast(kCGWindowLayer)];
      if (!layer)
        continue;

      NSInteger layer_integer = [layer integerValue];
      if (layer_integer == CGWindowLevelForKey(kCGNormalWindowLevelKey) ||
          layer_integer == CGWindowLevelForKey(kCGDockWindowLevelKey)) {
        return true;
      }
    }
    return false;
  }

  // Screen capture is always allowed in older macOS versions.
  return true;
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

void RequestSystemAudioCapturePermisson(base::OnceClosure callback,
                                        const base::TaskTraits& traits) {
  RequestSystemMediaCapturePermission(
      AVMediaTypeAudio, base::AdaptCallbackForRepeating(std::move(callback)),
      traits);
}

void RequestSystemVideoCapturePermisson(base::OnceClosure callback,
                                        const base::TaskTraits& traits) {
  RequestSystemMediaCapturePermission(
      AVMediaTypeVideo, base::AdaptCallbackForRepeating(std::move(callback)),
      traits);
}

void SetMediaAuthorizationWrapperForTesting(
    MediaAuthorizationWrapper* wrapper) {
  CHECK(!g_media_authorization_wrapper_for_tests);
  g_media_authorization_wrapper_for_tests = wrapper;
}

}  // namespace system_media_permissions
