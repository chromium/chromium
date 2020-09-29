// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These "headers" actually contain several function definitions and thus can
// only be included once across Chromium.
#include "chrome/android/features/cablev2_authenticator/jni_headers/CableAuthenticator_jni.h"

using base::android::ScopedJavaLocalRef;
using base::android::JavaParamRef;

// These functions are the entry points for BLEHandler.java calling into C++.

static void JNI_CableAuthenticator_Start(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    const JavaParamRef<jbyteArray>& state_bytes) {
}

static void JNI_CableAuthenticator_Stop(JNIEnv* env) {
}

static void JNI_CableAuthenticator_OnQRScanned(
    JNIEnv* env,
    const JavaParamRef<jstring>& jvalue) {
}

static ScopedJavaLocalRef<jobjectArray> JNI_CableAuthenticator_OnBLEWrite(
    JNIEnv* env,
    jlong client,
    jint mtu,
    const JavaParamRef<jbyteArray>& data) {
  return nullptr;
}

static void JNI_CableAuthenticator_OnAuthenticatorAttestationResponse(
    JNIEnv* env,
    jint ctap_status,
    const JavaParamRef<jbyteArray>& jclient_data_json,
    const JavaParamRef<jbyteArray>& jattestation_object) {
}

static void JNI_CableAuthenticator_OnAuthenticatorAssertionResponse(
    JNIEnv* env,
    jint ctap_status,
    const JavaParamRef<jbyteArray>& jclient_data_json,
    const JavaParamRef<jbyteArray>& jcredential_id,
    const JavaParamRef<jbyteArray>& jauthenticator_data,
    const JavaParamRef<jbyteArray>& jsignature) {
}
