// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_keyed_service_android.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
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

template <mojom::InvocationSource Source>
class AndroidAutoSubmitPasskeyHelper {
 public:
  static InvokeWithAutoSubmitPasskey GetPassKey() {
    return InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
  }
};

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
  allowed_changed_subscription_ = service_->enabling().RegisterAllowedChanged(
      base::BindRepeating(&GlicKeyedServiceAndroid::OnAllowedStateChanged,
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

bool GlicKeyedServiceAndroid::InvokeWithAutoSubmit(JNIEnv* env,
                                                   TabAndroid* tab,
                                                   std::string text,
                                                   int32_t source) {
  if (!tab) {
    return false;
  }

  auto invocation_source = static_cast<mojom::InvocationSource>(source);
  GlicInvokeOptions options(Target(tab), invocation_source);
  options.prompts.push_back(std::move(text));

  switch (invocation_source) {
    case mojom::InvocationSource::kUniversalCart:
      service_->InvokeWithAutoSubmit(
          AndroidAutoSubmitPasskeyHelper<
              mojom::InvocationSource::kUniversalCart>::GetPassKey(),
          std::move(options));
      return true;
    default:
      // Handle unauthorized source
      return false;
  }
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
  Java_GlicKeyedServiceImpl_onGlobalShowHide(env, java_obj_);
}

void GlicKeyedServiceAndroid::OnUserEnabledActuationOnWebChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool enabled = service_->enabling().GetUserEnabledActuationOnWeb();
  Java_GlicKeyedServiceImpl_onUserEnabledActuationOnWebChanged(env, java_obj_,
                                                               enabled);
}

void GlicKeyedServiceAndroid::OnAllowedStateChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicKeyedServiceImpl_onAllowedStateChanged(env, java_obj_);
}

bool GlicKeyedService::IsGlicShortcutActive() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_service = GetJavaObject(this);
  if (!java_service) {
    return false;
  }
  return Java_GlicKeyedServiceImpl_isGlicShortcutActive(
      env, java_service, profile_->GetJavaObject());
}

bool GlicKeyedService::IsBottomBarEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_service = GetJavaObject(this);
  if (!java_service) {
    return false;
  }
  return Java_GlicKeyedServiceImpl_isBottomBarEnabled(env, java_service);
}

}  // namespace glic

DEFINE_JNI(GlicKeyedServiceImpl)
