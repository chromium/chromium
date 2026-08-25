// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_MANAGER_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "services/network/public/cpp/resource_request.h"

namespace android_webview {

// Client implementation for managing interactions with the
// `ContentRestrictionManager` system service via the
// `AwContentRestrictionManagerBridge`.
class AwContentRestrictionManagerClient {
 public:
  using ContentClassificationCallback = base::OnceCallback<void(bool)>;

  // Delegate class used to manage all JNI interactions with the
  // `AwContentRestrictionManagerBridge`. Can be overridden to simplify testing.
  class Delegate {
   public:
    explicit Delegate(const base::android::JavaRef<jobject>& java_bridge);
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate();

    virtual bool IsContentRestrictionEnabled();
    virtual void RequestContentClassification(
        int64_t navigation_id,
        const std::string& url,
        const std::string& mime_type,
        ContentClassificationCallback callback);
    virtual bool SendShowRestrictedContentIntent(const GURL& url);
    virtual int CreateRequestBodyPipeAndGetWriteFd(int64_t navigation_id);

   private:
    base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
  };

  static std::unique_ptr<AwContentRestrictionManagerClient> Create();
  static std::unique_ptr<AwContentRestrictionManagerClient> CreateForTesting(
      std::unique_ptr<Delegate> delegate);

  AwContentRestrictionManagerClient(const AwContentRestrictionManagerClient&) =
      delete;
  AwContentRestrictionManagerClient& operator=(
      const AwContentRestrictionManagerClient&) = delete;
  virtual ~AwContentRestrictionManagerClient();

  // Returns true if the content restriction feature is enabled for WebViews.
  // False otherwise.
  bool IsContentRestrictionEnabled();

  // Requests content restriction classification for the given request and
  // invokes the callback with the classification result.
  void RequestContentClassification(int64_t navigation_id,
                                    const network::ResourceRequest& request,
                                    ContentClassificationCallback callback);

  // Sends an intent to the Android platform to display a dialog about the
  // restricted content. Returns true if the intent was sent successfully, false
  // otherwise.
  bool SendShowRestrictedContentIntent(const GURL& url);

  // Creates a pipe for streaming the request body and returns the write file
  // descriptor handle. Returns -1 on failure.
  int CreateRequestBodyPipeAndGetWriteFd(int64_t navigation_id);

 private:
  // Internal helper class to keep track of pending content restriction
  // classification requests while enforcing request timeouts.
  class ClassificationRequestTracker {
   public:
    ClassificationRequestTracker();
    ClassificationRequestTracker(const ClassificationRequestTracker&) = delete;
    ClassificationRequestTracker& operator=(
        const ClassificationRequestTracker&) = delete;
    ~ClassificationRequestTracker();

    // Registers a new classification request and sets up a timer to enforce a
    // timeout if the request is not completed within the timeout period.
    void RegisterClassificationRequest(int64_t navigation_id,
                                       ContentClassificationCallback callback);

    // Helper callback invoked when content has been classified.
    void OnClassificationResult(int64_t navigation_id, bool is_allowed);

    base::WeakPtr<ClassificationRequestTracker> GetWeakPtr();

   private:
    struct PendingRequest {
      ContentClassificationCallback callback;
      std::unique_ptr<base::OneShotTimer> timer;
    };

    // Internal timer callback invoked when the content classification request
    // times out.
    void OnTimeout(int64_t navigation_id);

    std::map<int64_t, PendingRequest> pending_requests_;

    base::WeakPtrFactory<ClassificationRequestTracker> weak_ptr_factory_{this};
  };

  explicit AwContentRestrictionManagerClient(
      std::unique_ptr<Delegate> delegate);

  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
  const std::unique_ptr<Delegate> delegate_;
  ClassificationRequestTracker request_tracker_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_MANAGER_CLIENT_H_
