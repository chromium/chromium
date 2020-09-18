// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/internal_authenticator_android.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/timer/timer.h"
#include "chrome/android/chrome_jni_headers/AuthenticatorImpl_jni.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "content/browser/webauth/authenticator_mojom_traits.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/mojom/base/time.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

InternalAuthenticatorAndroid::InternalAuthenticatorAndroid(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  DCHECK(render_frame_host_);
  JNIEnv* env = AttachCurrentThread();
  java_authenticator_impl_ref_ = Java_AuthenticatorImpl_create(
      env, reinterpret_cast<intptr_t>(this),
      render_frame_host_->GetJavaRenderFrameHost());
}

InternalAuthenticatorAndroid::~InternalAuthenticatorAndroid() {
  // This call exists to assert that |render_frame_host_| outlives this object.
  // If this is violated, ASAN should notice.
  DCHECK(render_frame_host_);
  render_frame_host_->GetRoutingID();
}

void InternalAuthenticatorAndroid::SetEffectiveOrigin(
    const url::Origin& origin) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  Java_AuthenticatorImpl_setEffectiveOrigin(env, obj,
                                            origin.CreateJavaObject());
}

void InternalAuthenticatorAndroid::MakeCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    blink::mojom::Authenticator::MakeCredentialCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  make_credential_response_callback_ = std::move(callback);

  std::vector<uint8_t> byte_vector =
      blink::mojom::PublicKeyCredentialCreationOptions::Serialize(&options);
  ScopedJavaLocalRef<jobject> byte_buffer = ScopedJavaLocalRef<jobject>(
      env, env->NewDirectByteBuffer(byte_vector.data(), byte_vector.size()));

  Java_AuthenticatorImpl_makeCredentialBridge(env, obj, byte_buffer);
}

void InternalAuthenticatorAndroid::GetAssertion(
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    blink::mojom::Authenticator::GetAssertionCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  get_assertion_response_callback_ = std::move(callback);

  std::vector<uint8_t> byte_vector =
      blink::mojom::PublicKeyCredentialRequestOptions::Serialize(&options);
  ScopedJavaLocalRef<jobject> byte_buffer = ScopedJavaLocalRef<jobject>(
      env, env->NewDirectByteBuffer(byte_vector.data(), byte_vector.size()));

  Java_AuthenticatorImpl_getAssertionBridge(env, obj, byte_buffer);
}

void InternalAuthenticatorAndroid::
    IsUserVerifyingPlatformAuthenticatorAvailable(
        blink::mojom::Authenticator::
            IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  is_uvpaa_callback_ = std::move(callback);
  Java_AuthenticatorImpl_isUserVerifyingPlatformAuthenticatorAvailableBridge(
      env, obj);
}

void InternalAuthenticatorAndroid::Cancel() {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  Java_AuthenticatorImpl_cancel(env, obj);
}

content::RenderFrameHost* InternalAuthenticatorAndroid::GetRenderFrameHost() {
  return render_frame_host_;
}

void InternalAuthenticatorAndroid::InvokeMakeCredentialResponse(
    JNIEnv* env,
    jint status,
    const base::android::JavaParamRef<jobject>& byte_buffer) {
  blink::mojom::MakeCredentialAuthenticatorResponsePtr response;

  // |byte_buffer| may be null if authentication failed.
  if (byte_buffer) {
    blink::mojom::MakeCredentialAuthenticatorResponse::Deserialize(
        std::move(payments::android::JavaByteBufferToNativeByteVector(
            env, byte_buffer)),
        &response);
  }

  std::move(make_credential_response_callback_)
      .Run(static_cast<blink::mojom::AuthenticatorStatus>(status),
           std::move(response));
}

void InternalAuthenticatorAndroid::InvokeGetAssertionResponse(
    JNIEnv* env,
    jint status,
    const base::android::JavaParamRef<jobject>& byte_buffer) {
  blink::mojom::GetAssertionAuthenticatorResponsePtr response;

  // |byte_buffer| may be null if authentication failed.
  if (byte_buffer) {
    blink::mojom::GetAssertionAuthenticatorResponse::Deserialize(
        std::move(payments::android::JavaByteBufferToNativeByteVector(
            env, byte_buffer)),
        &response);
  }

  std::move(get_assertion_response_callback_)
      .Run(static_cast<blink::mojom::AuthenticatorStatus>(status),
           std::move(response));
}

void InternalAuthenticatorAndroid::
    InvokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
        JNIEnv* env,
        jboolean is_uvpaa) {
  std::move(is_uvpaa_callback_).Run(static_cast<bool>(is_uvpaa));
}

JavaRef<jobject>& InternalAuthenticatorAndroid::GetJavaObject() {
  if (java_authenticator_impl_ref_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    java_authenticator_impl_ref_ = Java_AuthenticatorImpl_create(
        env, reinterpret_cast<intptr_t>(this),
        render_frame_host_->GetJavaRenderFrameHost());
  }
  return java_authenticator_impl_ref_;
}
