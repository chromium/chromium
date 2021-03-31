// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/subscriptions/commerce_subscription_db.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "chrome/browser/commerce/subscriptions/android/jni_headers/CommerceSubscription_jni.h"
#include "chrome/browser/commerce/subscriptions/android/jni_headers/CommerceSubscriptionsStorage_jni.h"
#include "chrome/browser/commerce/subscriptions/commerce_subscription_db_content.pb.h"
#include "chrome/browser/persisted_state_db/profile_proto_db_factory.h"
#include "components/embedder_support/android/browser_context/browser_context_handle.h"

namespace {

using CommerceSubscriptionProto =
    commerce_subscription_db::CommerceSubscriptionContentProto;
using CommerceSubscriptions =
    std::vector<ProfileProtoDB<CommerceSubscriptionProto>::KeyAndValue>;
using SubscriptionManagementTypeProto = commerce_subscription_db::
    CommerceSubscriptionContentProto_SubscriptionManagementType;
using SubscriptionTypeProto =
    commerce_subscription_db::CommerceSubscriptionContentProto_SubscriptionType;
using TrackingIdTypeProto =
    commerce_subscription_db::CommerceSubscriptionContentProto_TrackingIdType;

SubscriptionManagementTypeProto getManagementTypeForString(
    const std::string& management_type_string) {
  static constexpr auto stringToManagementTypeMap = base::MakeFixedFlatMap<
      base::StringPiece, SubscriptionManagementTypeProto>(
      {{"TYPE_UNSPECIFIED",
        commerce_subscription_db::
            CommerceSubscriptionContentProto_SubscriptionManagementType_MANAGE_TYPE_UNSPECIFIED},
       {"CHROME_MANAGED",
        commerce_subscription_db::
            CommerceSubscriptionContentProto_SubscriptionManagementType_CHROME_MANAGED},
       {"USER_MANAGED",
        commerce_subscription_db::
            CommerceSubscriptionContentProto_SubscriptionManagementType_USER_MANAGED}});
  return stringToManagementTypeMap.at(management_type_string);
}

const base::StringPiece& getStringForManagementType(
    SubscriptionManagementTypeProto management_type) {
  static constexpr auto managementTypeToStringMap = base::MakeFixedFlatMap<
      SubscriptionManagementTypeProto, base::StringPiece>(
      {{commerce_subscription_db::
            CommerceSubscriptionContentProto_SubscriptionManagementType_MANAGE_TYPE_UNSPECIFIED,
        "TYPE_UNSPECIFIED"},
       {commerce_subscription_db::
            CommerceSubscriptionContentProto_SubscriptionManagementType_CHROME_MANAGED,
        "CHROME_MANAGED"},
       {commerce_subscription_db::
            CommerceSubscriptionContentProto_SubscriptionManagementType_USER_MANAGED,
        "USER_MANAGED"}});
  return managementTypeToStringMap.at(management_type);
}

SubscriptionTypeProto getSubscriptionTypeForString(
    const std::string& subscription_type_string) {
  SubscriptionTypeProto subscription_type = commerce_subscription_db::
      CommerceSubscriptionContentProto_SubscriptionType_TYPE_UNSPECIFIED;
  DCHECK(commerce_subscription_db::
             CommerceSubscriptionContentProto_SubscriptionType_Parse(
                 subscription_type_string, &subscription_type));
  return subscription_type;
}

const std::string& getStringForSubscriptionType(
    SubscriptionTypeProto subscription_type) {
  return commerce_subscription_db::
      CommerceSubscriptionContentProto_SubscriptionType_Name(subscription_type);
}

TrackingIdTypeProto getTrackingIdTypeForString(
    const std::string& tracking_id_type_string) {
  TrackingIdTypeProto tracking_id_type = commerce_subscription_db::
      CommerceSubscriptionContentProto_TrackingIdType_IDENTIFIER_TYPE_UNSPECIFIED;
  DCHECK(commerce_subscription_db::
             CommerceSubscriptionContentProto_TrackingIdType_Parse(
                 tracking_id_type_string, &tracking_id_type));
  return tracking_id_type;
}

const std::string& getStringForTrackingIdType(
    TrackingIdTypeProto tracking_id_type) {
  return commerce_subscription_db::
      CommerceSubscriptionContentProto_TrackingIdType_Name(tracking_id_type);
}

void OnLoadCallbackSingleEntry(const base::android::JavaRef<jobject>& jcallback,
                               bool success,
                               CommerceSubscriptions data) {
  DCHECK(success) << "There was an error loading from CommerceSubscriptionDB";
  if (data.size() == 0) {
    base::android::RunObjectCallbackAndroid(jcallback, nullptr);
    return;
  }
  DCHECK(data.size() == 1);
  CommerceSubscriptionProto proto = std::move(data.at(0).second);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> subscription =
      Java_CommerceSubscription_Constructor(
          env,
          base::android::ConvertUTF8ToJavaString(
              env, getStringForSubscriptionType(
                       proto.subscription_type())) /*subscription_type*/,
          base::android::ConvertUTF8ToJavaString(
              env, proto.tracking_id()) /*tracking_id*/,
          base::android::ConvertUTF8ToJavaString(
              env, getStringForManagementType(
                       proto.management_type())) /*management_type*/,
          base::android::ConvertUTF8ToJavaString(
              env, getStringForTrackingIdType(
                       proto.tracking_id_type())) /*tracking_id_type*/,
          proto.timestamp() /*timestamp*/);
  base::android::RunObjectCallbackAndroid(jcallback, subscription);
}

void OnLoadCallbackMultipleEntry(
    const base::android::JavaRef<jobject>& jcallback,
    bool success,
    CommerceSubscriptions data) {
  DCHECK(success) << "There was an error loading from CommerceSubscriptionDB";
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jlist =
      Java_CommerceSubscription_createSubscriptionList(env);
  for (ProfileProtoDB<CommerceSubscriptionProto>::KeyAndValue& kv : data) {
    CommerceSubscriptionProto proto = std::move(kv.second);
    Java_CommerceSubscription_createSubscriptionAndAddToList(
        env, jlist,
        base::android::ConvertUTF8ToJavaString(
            env, getStringForSubscriptionType(
                     proto.subscription_type())) /*subscription_type*/,
        base::android::ConvertUTF8ToJavaString(
            env, proto.tracking_id()) /*tracking_id*/,
        base::android::ConvertUTF8ToJavaString(
            env, getStringForManagementType(
                     proto.management_type())) /*management_type*/,
        base::android::ConvertUTF8ToJavaString(
            env, getStringForTrackingIdType(
                     proto.tracking_id_type())) /*tracking_id_type*/,
        proto.timestamp());
  }
  base::android::RunObjectCallbackAndroid(jcallback, jlist);
}

void OnUpdateCallback(
    const base::android::JavaRef<jobject>& joncomplete_for_testing,
    bool success) {
  DCHECK(success) << "There was an error modifying CommerceSubscriptionDB";
  if (joncomplete_for_testing)
    base::android::RunRunnableAndroid(joncomplete_for_testing);
}
}  // namespace

CommerceSubscriptionDB::CommerceSubscriptionDB(
    content::BrowserContext* browser_context)
    : proto_db_(ProfileProtoDBFactory<CommerceSubscriptionProto>::GetInstance()
                    ->GetForProfile(browser_context)) {}
CommerceSubscriptionDB::~CommerceSubscriptionDB() = default;

void CommerceSubscriptionDB::Save(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jstring>& jtype,
    const base::android::JavaParamRef<jstring>& jtracking_id,
    const base::android::JavaParamRef<jstring>& jmanagement_type,
    const base::android::JavaParamRef<jstring>& jtracking_id_type,
    const jlong jtimestamp,
    const base::android::JavaParamRef<jobject>& jcallback) {
  const std::string& key = base::android::ConvertJavaStringToUTF8(env, jkey);
  CommerceSubscriptionProto proto;
  proto.set_key(key);
  proto.set_tracking_id(base::android::ConvertJavaStringToUTF8(jtracking_id));
  proto.set_subscription_type(getSubscriptionTypeForString(
      base::android::ConvertJavaStringToUTF8(jtype)));
  proto.set_tracking_id_type(getTrackingIdTypeForString(
      base::android::ConvertJavaStringToUTF8(jtracking_id_type)));
  proto.set_management_type(getManagementTypeForString(
      base::android::ConvertJavaStringToUTF8(jmanagement_type)));
  proto.set_timestamp(jtimestamp);
  proto_db_->InsertContent(
      key, proto,
      base::BindOnce(&OnUpdateCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

void CommerceSubscriptionDB::Load(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jobject>& jcallback) {
  proto_db_->LoadOneEntry(
      base::android::ConvertJavaStringToUTF8(env, jkey),
      base::BindOnce(&OnLoadCallbackSingleEntry,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

void CommerceSubscriptionDB::LoadWithPrefix(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jprefix,
    const base::android::JavaParamRef<jobject>& jcallback) {
  proto_db_->LoadContentWithPrefix(
      base::android::ConvertJavaStringToUTF8(env, jprefix),
      base::BindOnce(&OnLoadCallbackMultipleEntry,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

void CommerceSubscriptionDB::Delete(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jobject>& joncomplete_for_testing) {
  proto_db_->DeleteOneEntry(
      base::android::ConvertJavaStringToUTF8(env, jkey),
      base::BindOnce(&OnUpdateCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(
                         joncomplete_for_testing)));
}

void CommerceSubscriptionDB::DeleteAll(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& joncomplete_for_testing) {
  proto_db_->DeleteAllContent(base::BindOnce(
      &OnUpdateCallback,
      base::android::ScopedJavaGlobalRef<jobject>(joncomplete_for_testing)));
}

void CommerceSubscriptionDB::Destroy(JNIEnv* env) {
  proto_db_->Destroy();
}

static void JNI_CommerceSubscriptionsStorage_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Java_CommerceSubscriptionsStorage_setNativePtr(
      env, obj,
      reinterpret_cast<intptr_t>(new CommerceSubscriptionDB(
          browser_context::BrowserContextFromJavaHandle(jprofile))));
}
