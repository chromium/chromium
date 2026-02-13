// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_keyed_service_android.h"

#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicKeyedServiceImpl_jni.h"

using base::android::AttachCurrentThread;

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace glic {
namespace {
const char kGlicKeyedServiceAndroidKey[] = "glic_keyed_service_android";
}  // namespace

// static
ScopedJavaLocalRef<jobject> GlicKeyedService::GetJavaObject(
    GlicKeyedService* service) {
  if (!service->GetUserData(kGlicKeyedServiceAndroidKey)) {
    service->SetUserData(kGlicKeyedServiceAndroidKey,
                         std::make_unique<GlicKeyedServiceAndroid>(service));
  }

  GlicKeyedServiceAndroid* bridge = static_cast<GlicKeyedServiceAndroid*>(
      service->GetUserData(kGlicKeyedServiceAndroidKey));

  return bridge->GetJavaObject();
}

GlicKeyedServiceAndroid::GlicKeyedServiceAndroid(GlicKeyedService* service)
    : service_(service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_GlicKeyedServiceImpl_create(
                           env, reinterpret_cast<int64_t>(this)));
}

GlicKeyedServiceAndroid::~GlicKeyedServiceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicKeyedServiceImpl_onNativeDestroyed(env, java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
GlicKeyedServiceAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void GlicKeyedServiceAndroid::ToggleUI(JNIEnv* env,
                                       int64_t browser_window_ptr,
                                       Profile* profile,
                                       int32_t source) {
  auto* window = reinterpret_cast<BrowserWindowInterface*>(browser_window_ptr);
  CHECK(window);

  service_->ToggleUI(window, /*prevent_close=*/false,
                     static_cast<mojom::InvocationSource>(source));
}

}  // namespace glic

DEFINE_JNI(GlicKeyedServiceImpl)
