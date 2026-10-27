// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/android/ttc_keyed_service_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ttc/android/jni_headers/TtcKeyedServiceFactory_jni.h"
#include "chrome/browser/ttc/android/jni_headers/TtcKeyedService_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace ttc {

namespace {
const char kTtcKeyedServiceBridgeKey[] = "ttc_keyed_service_bridge";
}  // namespace

// static
TtcKeyedServiceAndroid& TtcKeyedServiceAndroid::Get(TtcKeyedService& service) {
  auto* bridge = static_cast<TtcKeyedServiceAndroid*>(
      service.GetUserData(kTtcKeyedServiceBridgeKey));
  if (!bridge) {
    service.SetUserData(kTtcKeyedServiceBridgeKey,
                        std::make_unique<TtcKeyedServiceAndroid>(service));
    bridge = static_cast<TtcKeyedServiceAndroid*>(
        service.GetUserData(kTtcKeyedServiceBridgeKey));
  }
  return CHECK_DEREF(bridge);
}

ScopedJavaLocalRef<jobject> JNI_TtcKeyedServiceFactory_GetForProfile(
    Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  // Null when the feature is disabled for this profile.
  TtcKeyedService* service =
      TtcKeyedServiceFactory::GetTtcKeyedService(profile);
  if (!service) {
    return nullptr;
  }
  return TtcKeyedServiceAndroid::Get(*service).GetJavaObject();
}

TtcKeyedServiceAndroid::SessionObserver::SessionObserver(
    TtcKeyedServiceAndroid& bridge)
    : bridge_(bridge) {}

TtcKeyedServiceAndroid::SessionObserver::~SessionObserver() = default;

void TtcKeyedServiceAndroid::SessionObserver::OnSessionLifecycleChanged(
    SessionLifecycle lifecycle) {
  bridge_->OnSessionLifecycleChanged(lifecycle);
}

TtcKeyedServiceAndroid::TtcKeyedServiceAndroid(TtcKeyedService& service)
    : service_(service) {
  JNIEnv* env = AttachCurrentThread();
  java_obj_.Reset(
      env, Java_TtcKeyedService_create(env, reinterpret_cast<int64_t>(this)));
  state_changed_subscription_ = service_->RegisterStateChangedCallback(
      base::BindRepeating(&TtcKeyedServiceAndroid::OnServiceStateChanged,
                          base::Unretained(this)));
  // The bridge is created lazily, so a session may already be running.
  MaybeObserveSession();
}

TtcKeyedServiceAndroid::~TtcKeyedServiceAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_TtcKeyedService_clearNativePtr(env, java_obj_);
}

ScopedJavaLocalRef<jobject> TtcKeyedServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

jboolean TtcKeyedServiceAndroid::IsEnabled(JNIEnv* env) {
  return service_->IsEnabled();
}

jboolean TtcKeyedServiceAndroid::IsSessionActive(JNIEnv* env) {
  return service_->is_session_active();
}

int32_t TtcKeyedServiceAndroid::GetSessionLifecycle(JNIEnv* env) {
  SessionController* controller = service_->session_controller();
  return static_cast<int32_t>(controller ? controller->GetSessionLifecycle()
                                         : SessionLifecycle::kFinished);
}

void TtcKeyedServiceAndroid::StartSession(JNIEnv* env) {
  if (!service_->IsEnabled() || service_->is_session_active()) {
    return;
  }
  service_->StartSession();
}

void TtcKeyedServiceAndroid::EndSession(JNIEnv* env) {
  service_->EndSession();
}

bool TtcKeyedServiceAndroid::SetMicMuted(JNIEnv* env, bool muted) {
  return service_->SetMicMuted(muted);
}

bool TtcKeyedServiceAndroid::IsMicMuted(JNIEnv* env) {
  return service_->IsMicMuted();
}

void TtcKeyedServiceAndroid::OnAudioLevelChanged(float audio_level) {
  JNIEnv* env = AttachCurrentThread();
  Java_TtcKeyedService_onAudioLevelChanged(env, java_obj_, audio_level);
}

void TtcKeyedServiceAndroid::OnError(ErrorCode error) {
  JNIEnv* env = AttachCurrentThread();
  Java_TtcKeyedService_onError(env, java_obj_, static_cast<int32_t>(error));
}

void TtcKeyedServiceAndroid::OnServiceStateChanged(ServiceState state) {
  if (state == ServiceState::kSessionActive) {
    MaybeObserveSession();
  }
  JNIEnv* env = AttachCurrentThread();
  Java_TtcKeyedService_onServiceStateChanged(env, java_obj_,
                                             static_cast<int32_t>(state));
}

void TtcKeyedServiceAndroid::MaybeObserveSession() {
  SessionController* controller = service_->session_controller();
  if (controller && !session_observation_.IsObserving()) {
    // Reports the session's current lifecycle synchronously.
    session_observation_.Observe(controller);
  }
}

void TtcKeyedServiceAndroid::OnSessionLifecycleChanged(
    SessionLifecycle lifecycle) {
  // Every session reports kFinished exactly once before it's destroyed
  // (SessionControllerImpl's destructor reports it if EndSessionAsync() hasn't
  // already), so stop observing here rather than after it's gone.
  if (lifecycle == SessionLifecycle::kFinished) {
    session_observation_.Reset();
  }
  JNIEnv* env = AttachCurrentThread();
  Java_TtcKeyedService_onSessionLifecycleChanged(
      env, java_obj_, static_cast<int32_t>(lifecycle));
}

}  // namespace ttc

DEFINE_JNI(TtcKeyedService)
DEFINE_JNI(TtcKeyedServiceFactory)
