// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_keyed_service_android.h"

#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
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
  global_show_hide_subscription_ =
      service_->instance_coordinator().AddGlobalShowHideCallback(
          base::BindRepeating(&GlicKeyedServiceAndroid::OnGlobalShowHide,
                              base::Unretained(this)));
  web_actuation_pref_subscription_ =
      service_->enabling().RegisterOnUserEnabledActuationOnWebChanged(
          base::BindRepeating(
              &GlicKeyedServiceAndroid::OnUserEnabledActuationOnWebChanged,
              base::Unretained(this)));
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
                                       bool prevent_close,
                                       Profile* profile,
                                       int32_t source) {
  auto* window = reinterpret_cast<BrowserWindowInterface*>(browser_window_ptr);
  CHECK(window);

  service_->ToggleUI(window, prevent_close,
                     static_cast<mojom::InvocationSource>(source));
}

bool GlicKeyedServiceAndroid::IsPanelShowingForBrowser(
    JNIEnv* env,
    int64_t browser_window_ptr) {
  auto* window = reinterpret_cast<BrowserWindowInterface*>(browser_window_ptr);
  CHECK(window);
  return service_->IsPanelShowingForBrowser(*window);
}

bool GlicKeyedServiceAndroid::GetUserEnabledActuationOnWeb(JNIEnv* env) {
  return service_->enabling().GetUserEnabledActuationOnWeb();
}

void GlicKeyedServiceAndroid::SetUserEnabledActuationOnWeb(JNIEnv* env,
                                                           bool enabled) {
  service_->enabling().SetUserEnabledActuationOnWeb(enabled);
}

void GlicKeyedServiceAndroid::OnGlobalShowHide() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool is_opened = service_->IsWindowShowing();
  Java_GlicKeyedServiceImpl_onGlobalShowHide(env, java_obj_, is_opened);
}

void GlicKeyedServiceAndroid::OnUserEnabledActuationOnWebChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool enabled = service_->enabling().GetUserEnabledActuationOnWeb();
  Java_GlicKeyedServiceImpl_onUserEnabledActuationOnWebChanged(env, java_obj_,
                                                               enabled);
}

}  // namespace glic

DEFINE_JNI(GlicKeyedServiceImpl)
