// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"

#include "android_webview/common/aw_features.h"
#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwContentRestrictionManagerBridge_jni.h"

namespace android_webview {

AwContentRestrictionManagerClient::Delegate::Delegate(
    const base::android::JavaRef<jobject>& java_bridge)
    : java_bridge_(java_bridge) {}

AwContentRestrictionManagerClient::Delegate::~Delegate() = default;

bool AwContentRestrictionManagerClient::Delegate::
    IsContentRestrictionEnabled() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AwContentRestrictionManagerBridge_isContentRestrictionEnabled(
      env, java_bridge_);
}

void AwContentRestrictionManagerClient::Delegate::RequestContentClassification(
    int64_t navigation_id,
    const std::string& url,
    const std::string& mime_type,
    ContentClassificationCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwContentRestrictionManagerBridge_requestContentClassification(
      env, java_bridge_, navigation_id, url, mime_type,
      base::android::ToJniCallback(env, std::move(callback)));
}

bool AwContentRestrictionManagerClient::Delegate::
    SendShowRestrictedContentIntent(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AwContentRestrictionManagerBridge_sendShowRestrictedContentIntent(
      env, java_bridge_, url.spec());
}

int AwContentRestrictionManagerClient::Delegate::
    CreateRequestBodyPipeAndGetWriteFd(int64_t navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AwContentRestrictionManagerBridge_createRequestBodyPipeAndGetWriteFd(
      env, java_bridge_, navigation_id);
}

AwContentRestrictionManagerClient::ClassificationRequestTracker::
    ClassificationRequestTracker() = default;

AwContentRestrictionManagerClient::ClassificationRequestTracker::
    ~ClassificationRequestTracker() = default;

void AwContentRestrictionManagerClient::ClassificationRequestTracker::
    RegisterClassificationRequest(int64_t navigation_id,
                                  ContentClassificationCallback callback) {
  std::unique_ptr<base::OneShotTimer> timer =
      std::make_unique<base::OneShotTimer>();
  const base::TimeDelta timeout =
      features::kWebViewContentRestrictionTimeout.Get();
  base::OneShotTimer* const timer_ptr = timer.get();
  pending_requests_.insert_or_assign(
      navigation_id, PendingRequest{.callback = std::move(callback),
                                    .timer = std::move(timer)});

  timer_ptr->Start(
      FROM_HERE, timeout,
      base::BindOnce(&ClassificationRequestTracker::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr(), navigation_id));
}

void AwContentRestrictionManagerClient::ClassificationRequestTracker::
    OnClassificationResult(int64_t navigation_id, bool is_allowed) {
  auto it = pending_requests_.find(navigation_id);
  if (it == pending_requests_.end()) {
    // Request already timed out.
    return;
  }

  ContentClassificationCallback callback = std::move(it->second.callback);
  pending_requests_.erase(it);
  std::move(callback).Run(is_allowed);
}

void AwContentRestrictionManagerClient::ClassificationRequestTracker::OnTimeout(
    int64_t navigation_id) {
  auto it = pending_requests_.find(navigation_id);
  if (it == pending_requests_.end()) {
    // Should be rare, but we return regardless.
    return;
  }

  // The platform enforces its own timeouts so we should rarely get here. This
  // is only triggered when the platform is non-responsive, so we conservatively
  // fail open.
  ContentClassificationCallback callback = std::move(it->second.callback);
  pending_requests_.erase(it);
  std::move(callback).Run(true);
}

base::WeakPtr<AwContentRestrictionManagerClient::ClassificationRequestTracker>
AwContentRestrictionManagerClient::ClassificationRequestTracker::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
std::unique_ptr<AwContentRestrictionManagerClient>
AwContentRestrictionManagerClient::Create() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> java_bridge(
      Java_AwContentRestrictionManagerBridge_Constructor(env));
  auto delegate = std::make_unique<Delegate>(java_bridge);
  std::unique_ptr<AwContentRestrictionManagerClient> client = base::WrapUnique(
      new AwContentRestrictionManagerClient(std::move(delegate)));
  client->java_bridge_ = std::move(java_bridge);

  return client;
}

// static
std::unique_ptr<AwContentRestrictionManagerClient>
AwContentRestrictionManagerClient::CreateForTesting(
    std::unique_ptr<Delegate> delegate) {
  return base::WrapUnique(
      new AwContentRestrictionManagerClient(std::move(delegate)));
}

AwContentRestrictionManagerClient::AwContentRestrictionManagerClient(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

AwContentRestrictionManagerClient::~AwContentRestrictionManagerClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!java_bridge_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwContentRestrictionManagerBridge_destroy(env, java_bridge_);
  }
}

bool AwContentRestrictionManagerClient::IsContentRestrictionEnabled() {
  return delegate_->IsContentRestrictionEnabled();
}

void AwContentRestrictionManagerClient::RequestContentClassification(
    int64_t navigation_id,
    const network::ResourceRequest& request,
    ContentClassificationCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string mime_type;
  auto header_value =
      request.headers.GetHeader(net::HttpRequestHeaders::kContentType);
  if (header_value) {
    mime_type = *header_value;
  }

  request_tracker_.RegisterClassificationRequest(navigation_id,
                                                 std::move(callback));
  delegate_->RequestContentClassification(
      navigation_id, request.url.spec(), mime_type,
      base::BindOnce(&ClassificationRequestTracker::OnClassificationResult,
                     request_tracker_.GetWeakPtr(), navigation_id));
}

bool AwContentRestrictionManagerClient::SendShowRestrictedContentIntent(
    const GURL& url) {
  return delegate_->SendShowRestrictedContentIntent(url);
}

int AwContentRestrictionManagerClient::CreateRequestBodyPipeAndGetWriteFd(
    int64_t navigation_id) {
  return delegate_->CreateRequestBodyPipeAndGetWriteFd(navigation_id);
}

}  // namespace android_webview
