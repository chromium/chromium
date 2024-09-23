// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/merchant_viewer/merchant_signal_db.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "components/commerce/core/proto/merchant_signal_db_content.pb.h"
#include "content/public/browser/android/browser_context_handle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/commerce/merchant_viewer/android/jni_headers/MerchantTrustSignalsEventStorage_jni.h"
#include "chrome/browser/commerce/merchant_viewer/android/jni_headers/MerchantTrustSignalsEvent_jni.h"

namespace {

using MerchantSignalProto = merchant_signal_db::MerchantSignalContentProto;
using MerchantSignals =
    std::vector<SessionProtoDB<MerchantSignalProto>::KeyAndValue>;

void OnLoadCallbackSingleEntry(const base::android::JavaRef<jobject>& jcallback,
                               bool success,
                               MerchantSignals data) {
  DCHECK(success) << "There was an error loading from MerchantSignalDB";
  if (data.size() == 0) {
    base::android::RunObjectCallbackAndroid(jcallback, nullptr);
    return;
  }
  DCHECK(data.size() == 1);
  MerchantSignalProto proto = std::move(data.at(0).second);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> signal =
      Java_MerchantTrustSignalsEvent_Constructor(
          env, base::android::ConvertUTF8ToJavaString(env, proto.key()) /*key*/,
          proto.trust_signals_message_displayed_timestamp() /*timestamp*/);
  base::android::RunObjectCallbackAndroid(jcallback, signal);
}

void OnLoadCallbackMultipleEntry(
    const base::android::JavaRef<jobject>& jcallback,
    bool success,
    MerchantSignals data) {
  DCHECK(success) << "There was an error loading from MerchantSignalDB";
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jlist =
      Java_MerchantTrustSignalsEvent_createEventList(env);
  for (SessionProtoDB<MerchantSignalProto>::KeyAndValue& kv : data) {
    MerchantSignalProto proto = std::move(kv.second);
    Java_MerchantTrustSignalsEvent_createEventAndAddToList(
        env, jlist,
        base::android::ConvertUTF8ToJavaString(env, proto.key()) /*key*/,
        proto.trust_signals_message_displayed_timestamp() /*timestamp*/);
  }
  base::android::RunObjectCallbackAndroid(jcallback, jlist);
}

void OnUpdateCallback(
    const base::android::JavaRef<jobject>& joncomplete_for_testing,
    bool success) {
  DCHECK(success) << "There was an error modifying MerchantSignalDB";
  if (joncomplete_for_testing)
    base::android::RunRunnableAndroid(joncomplete_for_testing);
}
}  // namespace

MerchantSignalDB::MerchantSignalDB(content::BrowserContext* browser_context)
    : proto_db_(SessionProtoDBFactory<MerchantSignalProto>::GetInstance()
                    ->GetForProfile(browser_context)) {}
MerchantSignalDB::~MerchantSignalDB() = default;

void MerchantSignalDB::Save(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const jlong jtimestamp,
    const base::android::JavaParamRef<jobject>& jcallback) {
  const std::string& key = base::android::ConvertJavaStringToUTF8(env, jkey);
  MerchantSignalProto proto;
  proto.set_key(key);
  proto.set_trust_signals_message_displayed_timestamp(jtimestamp);
  proto_db_->InsertContent(
      key, proto,
      base::BindOnce(&OnUpdateCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

void MerchantSignalDB::Load(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jobject>& jcallback) {
  proto_db_->LoadOneEntry(
      base::android::ConvertJavaStringToUTF8(env, jkey),
      base::BindOnce(&OnLoadCallbackSingleEntry,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

void MerchantSignalDB::LoadWithPrefix(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jprefix,
    const base::android::JavaParamRef<jobject>& jcallback) {
  proto_db_->LoadContentWithPrefix(
      base::android::ConvertJavaStringToUTF8(env, jprefix),
      base::BindOnce(&OnLoadCallbackMultipleEntry,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

void MerchantSignalDB::Delete(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jobject>& joncomplete_for_testing) {
  proto_db_->DeleteOneEntry(
      base::android::ConvertJavaStringToUTF8(env, jkey),
      base::BindOnce(&OnUpdateCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(
                         joncomplete_for_testing)));
}

void MerchantSignalDB::DeleteAll(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& joncomplete_for_testing) {
  proto_db_->DeleteAllContent(base::BindOnce(
      &OnUpdateCallback,
      base::android::ScopedJavaGlobalRef<jobject>(joncomplete_for_testing)));
}

static void JNI_MerchantTrustSignalsEventStorage_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Java_MerchantTrustSignalsEventStorage_setNativePtr(
      env, obj,
      reinterpret_cast<intptr_t>(new MerchantSignalDB(
          content::BrowserContextFromJavaHandle(jprofile))));
}
