// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/android/page_content_extraction_service_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/functional/bind.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/service_jni_headers/PageContentExtractionService_jni.h"
#include "components/page_content_annotations/core/page_content_cache.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace page_content_annotations {
namespace {

const char kPageContentExtractionServiceBridgeKey[] =
    "page_content_extraction_service_bridge";

}  // namespace

// static
ScopedJavaLocalRef<jobject> PageContentExtractionService::GetJavaObject(
    PageContentExtractionService* service) {
  if (!service->GetUserData(kPageContentExtractionServiceBridgeKey)) {
    service->SetUserData(
        kPageContentExtractionServiceBridgeKey,
        std::make_unique<PageContentExtractionServiceAndroid>(service));
  }

  PageContentExtractionServiceAndroid* bridge =
      static_cast<PageContentExtractionServiceAndroid*>(
          service->GetUserData(kPageContentExtractionServiceBridgeKey));

  return bridge->GetJavaObject();
}

PageContentExtractionServiceAndroid::PageContentExtractionServiceAndroid(
    PageContentExtractionService* service)
    : service_(service) {
  DCHECK(service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_PageContentExtractionService_create(
                           env, reinterpret_cast<int64_t>(this)));
}

PageContentExtractionServiceAndroid::~PageContentExtractionServiceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PageContentExtractionService_clearNativePtr(env, java_obj_);
}

void PageContentExtractionServiceAndroid::GetAllCachedTabIds(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_callback) {
  if (!service_ || !service_->GetPageContentCache()) {
    base::android::RunObjectCallbackAndroid(
        j_callback,
        base::android::ToJavaLongArray(env, std::vector<int64_t>()));
    return;
  }
  service_->GetPageContentCache()->GetAllTabIds(base::BindOnce(
      [](const base::android::JavaRef<jobject>& callback,
         std::vector<int64_t> tab_ids) {
        JNIEnv* env = base::android::AttachCurrentThread();
        base::android::RunObjectCallbackAndroid(
            callback, base::android::ToJavaLongArray(env, tab_ids));
      },
      base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

ScopedJavaLocalRef<jobject>
PageContentExtractionServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace page_content_annotations

DEFINE_JNI(PageContentExtractionService)
