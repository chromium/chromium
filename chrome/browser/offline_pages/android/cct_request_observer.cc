// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/cct_request_observer.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_int_wrapper.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/CCTRequestStatus_jni.h"
#include "chrome/browser/android/app_hooks.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"

namespace offline_pages {
namespace {
int kCCTRequestObserverUserDataKey;
}  // namespace

using chrome::android::AppHooks;

// static
void CCTRequestObserver::AttachToRequestCoordinator(
    RequestCoordinator* coordinator) {
  if (!IsOfflinePagesCTEnabled())
    return;

  base::android::ScopedJavaLocalRef<jobject> callback =
      AppHooks::GetOfflinePagesCCTRequestDoneCallback();
  if (!callback.obj())
    return;

  auto request_observer = base::WrapUnique(new CCTRequestObserver(callback));
  coordinator->AddObserver(request_observer.get());
  coordinator->SetUserData(&kCCTRequestObserverUserDataKey,
                           std::move(request_observer));
}

CCTRequestObserver::~CCTRequestObserver() = default;

CCTRequestObserver::CCTRequestObserver(
    base::android::ScopedJavaLocalRef<jobject> callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  j_callback_.Reset(env, callback.obj());
}

void CCTRequestObserver::OnAdded(const SavePageRequest& request) {}

void CCTRequestObserver::OnCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  if (request.client_id().name_space != kCCTNamespace) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> callback_info =
      Java_CCTRequestStatus_create(
          env, as_jint(static_cast<int>(status)),
          base::android::ConvertUTF8ToJavaString(env, request.client_id().id));

  base::android::RunObjectCallbackAndroid(j_callback_, callback_info);
}

void CCTRequestObserver::OnChanged(const SavePageRequest& request) {}

void CCTRequestObserver::OnNetworkProgress(const SavePageRequest& request,
                                           int64_t received_bytes) {}

}  // namespace offline_pages
