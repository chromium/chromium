// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_MANAGER_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_MANAGER_CLIENT_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "services/network/public/cpp/resource_request.h"

namespace android_webview {

// Client implementation for managing interactions with the
// `ContentRestrictionManager` system service via the
// `AwContentRestrictionManagerBridge`.
class AwContentRestrictionManagerClient {
 public:
  using ContentClassificationCallback = base::OnceCallback<void(bool)>;

  AwContentRestrictionManagerClient();
  AwContentRestrictionManagerClient(const AwContentRestrictionManagerClient&) =
      delete;
  AwContentRestrictionManagerClient& operator=(
      const AwContentRestrictionManagerClient&) = delete;
  virtual ~AwContentRestrictionManagerClient();

  // Returns true if the content restriction feature is enabled for WebViews.
  // False otherwise.
  virtual bool IsContentRestrictionEnabled();

  // Requests content restriction classification for the given request and
  // invokes the callback with the classification result.
  virtual void RequestContentClassification(
      int64_t navigation_id,
      const network::ResourceRequest& request,
      ContentClassificationCallback callback);

  // Sends an intent to the Android platform to display a dialog about the
  // restricted content. Returns true if the intent was sent successfully, false
  // otherwise.
  virtual bool SendShowRestrictedContentIntent(const GURL& url);

  // Creates a pipe for streaming the request body and returns the write file
  // descriptor handle. Returns -1 on failure.
  virtual int CreateRequestBodyPipeAndGetWriteFd(int64_t navigation_id);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_MANAGER_CLIENT_H_
