// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PERMISSION_MEDIA_ACCESS_PERMISSION_REQUEST_H_
#define ANDROID_WEBVIEW_BROWSER_PERMISSION_MEDIA_ACCESS_PERMISSION_REQUEST_H_

#include <stdint.h>

#include "android_webview/browser/permission/aw_permission_request_delegate.h"
#include "base/functional/callback.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace android_webview {
class AwPermissionManager;

// The AwPermissionRequestDelegate implementation for media access permission
// request.
// Lifetime: Temporary
class MediaAccessPermissionRequest : public AwPermissionRequestDelegate {
 public:
  MediaAccessPermissionRequest(const content::MediaStreamRequest& request,
                               content::MediaResponseCallback callback,
                               AwPermissionManager& permission_manager,
                               bool can_cache_file_url_permissions);

  MediaAccessPermissionRequest(const MediaAccessPermissionRequest&) = delete;
  MediaAccessPermissionRequest& operator=(const MediaAccessPermissionRequest&) =
      delete;

  ~MediaAccessPermissionRequest() override;

  // AwPermissionRequestDelegate implementation.
  const GURL& GetOrigin() override;
  int64_t GetResources() override;
  void NotifyRequestResult(bool allowed) override;

 private:
  friend class TestMediaAccessPermissionRequest;

  const content::MediaStreamRequest request_;
  content::MediaResponseCallback callback_;
  const raw_ref<AwPermissionManager> permission_manager_;
  bool can_cache_file_url_permissions_;

  // For test only.
  blink::MediaStreamDevices audio_test_devices_;
  blink::MediaStreamDevices video_test_devices_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PERMISSION_MEDIA_ACCESS_PERMISSION_REQUEST_H_
