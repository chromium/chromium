// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_actor_login_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_service_factory.h"
#include "chrome/browser/password_manager/factories/account_password_store_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager_impl.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/ActorLoginPermission_jni.h"
#include "chrome/browser/glic/android/jni_headers/GlicActorLoginBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace glic {

GlicActorLoginBridge::GlicActorLoginBridge(JNIEnv* env,
                                           const JavaRef<jobject>& obj,
                                           Profile* profile)
    : profile_(profile) {
  java_obj_.Reset(env, obj);

  manager_ = std::make_unique<actor_login::ActorLoginPermissionsManagerImpl>(
      AffiliationServiceFactory::GetForProfile(profile),
      actor_login::ActorLoginPermissionServiceFactory::GetForProfile(profile),
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS));
}

GlicActorLoginBridge::~GlicActorLoginBridge() = default;

void GlicActorLoginBridge::Destroy(JNIEnv* env) {
  delete this;
}

void GlicActorLoginBridge::GetAllPermissions(
    JNIEnv* env,
    const JavaRef<jobject>& jcallback) {
  manager_->GetAllPermissions(
      SyncServiceFactory::GetForProfile(profile_),
      base::BindOnce(&GlicActorLoginBridge::OnGetAllPermissionsComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

void GlicActorLoginBridge::OnGetAllPermissionsComplete(
    ScopedJavaGlobalRef<jobject> jcallback,
    base::flat_set<password_manager::ActorLoginPermission> permissions) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_list =
      Java_GlicActorLoginBridge_createPermissionList(env);

  for (const auto& permission : permissions) {
    ScopedJavaLocalRef<jobject> j_permission =
        Java_ActorLoginPermission_Constructor(
            env, ConvertUTF8ToJavaString(env, permission.domain_info.name),
            url::GURLAndroid::FromNativeGURL(env, permission.domain_info.url),
            ConvertUTF8ToJavaString(env, permission.domain_info.signon_realm),
            ConvertUTF16ToJavaString(env, permission.username),
            url::GURLAndroid::FromNativeGURL(env, permission.favicon_url));

    Java_GlicActorLoginBridge_addPermissionToList(env, j_list, j_permission);
  }

  base::android::RunObjectCallbackAndroid(jcallback, j_list);
}

void GlicActorLoginBridge::RevokePermission(
    JNIEnv* env,
    const JavaRef<jstring>& j_signon_realm,
    const JavaRef<jstring>& j_username,
    const JavaRef<jobject>& jcallback) {
  std::string signon_realm =
      base::android::ConvertJavaStringToUTF8(env, j_signon_realm);
  std::string username =
      base::android::ConvertJavaStringToUTF8(env, j_username);

  manager_->RevokePermission(
      signon_realm, username,
      base::BindOnce(&GlicActorLoginBridge::OnRevokePermissionComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

void GlicActorLoginBridge::OnRevokePermissionComplete(
    ScopedJavaGlobalRef<jobject> jcallback,
    bool success) {
  base::android::RunBooleanCallbackAndroid(jcallback, success);
}

static int64_t JNI_GlicActorLoginBridge_Init(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  GlicActorLoginBridge* bridge = new GlicActorLoginBridge(env, obj, profile);
  return reinterpret_cast<intptr_t>(bridge);
}

DEFINE_JNI(GlicActorLoginBridge)

}  // namespace glic
