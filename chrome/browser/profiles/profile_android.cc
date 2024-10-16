// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_key_android.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/profiles/android/jni_headers/Profile_jni.h"

using jni_zero::AttachCurrentThread;
using jni_zero::JavaParamRef;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;


// static
Profile* Profile::FromJavaObject(const JavaRef<jobject>& obj) {
  if (!obj) {
    return nullptr;
  }
  return reinterpret_cast<Profile*>(
      Java_Profile_getNativePointer(AttachCurrentThread(), obj));
}

void Profile::InitJavaObject() {
  // Java side assumes |this| can also be used as a BrowserContext*.
  CHECK(
      reinterpret_cast<intptr_t>(this) ==
      reinterpret_cast<intptr_t>(static_cast<content::BrowserContext*>(this)));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_otr_profile_id =
      otr_profile_id_.has_value()
          ? otr_profile_id_->ConvertToJavaOTRProfileID(env)
          : nullptr;
  j_obj_ = Java_Profile_Constructor(env, reinterpret_cast<intptr_t>(this),
                                    j_otr_profile_id);
}

void Profile::NotifyJavaOnProfileWillBeDestroyed() {
  Java_Profile_onProfileWillBeDestroyed(AttachCurrentThread(), j_obj_);
}

void Profile::DestroyJavaObject() {
  Java_Profile_onNativeDestroyed(AttachCurrentThread(), j_obj_);
}

ScopedJavaLocalRef<jobject> Profile::GetJavaObject() const {
  return ScopedJavaLocalRef<jobject>(AttachCurrentThread(), j_obj_);
}

ScopedJavaLocalRef<jobject> JNI_Profile_GetOriginalProfile(JNIEnv* env,
                                                           jlong ptr) {
  Profile* self = reinterpret_cast<Profile*>(ptr);
  Profile* original_profile = self->GetOriginalProfile();
  DCHECK(original_profile);
  return original_profile->GetJavaObject();
}

jboolean JNI_Profile_IsInitialProfile(JNIEnv* env, jlong ptr) {
  Profile* self = reinterpret_cast<Profile*>(ptr);
  return self->GetBaseName().value() == chrome::kInitialProfile;
}

ScopedJavaLocalRef<jobject> JNI_Profile_GetOffTheRecordProfile(
    JNIEnv* env,
    jlong ptr,
    const JavaParamRef<jobject>& j_otr_profile_id,
    const jboolean j_create_if_needed) {
  Profile* self = reinterpret_cast<Profile*>(ptr);
  Profile::OTRProfileID otr_profile_id =
      Profile::OTRProfileID::ConvertFromJavaOTRProfileID(env, j_otr_profile_id);
  Profile* otr_profile =
      self->GetOffTheRecordProfile(otr_profile_id, j_create_if_needed);
  if (!j_create_if_needed && !otr_profile) {
    return nullptr;
  }
  DCHECK(otr_profile);
  return otr_profile->GetJavaObject();
}

ScopedJavaLocalRef<jobject> JNI_Profile_GetPrimaryOTRProfile(
    JNIEnv* env,
    jlong ptr,
    const jboolean j_create_if_needed) {
  Profile* self = reinterpret_cast<Profile*>(ptr);
  Profile* otr_profile = self->GetPrimaryOTRProfile(j_create_if_needed);
  if (!j_create_if_needed && !otr_profile) {
    return nullptr;
  }
  DCHECK(otr_profile);
  return otr_profile->GetJavaObject();
}

jboolean JNI_Profile_HasOffTheRecordProfile(
    JNIEnv* env,
    jlong ptr,
    const JavaParamRef<jobject>& j_otr_profile_id) {
  Profile* self = reinterpret_cast<Profile*>(ptr);
  Profile::OTRProfileID otr_profile_id =
      Profile::OTRProfileID::ConvertFromJavaOTRProfileID(env, j_otr_profile_id);
  return self->HasOffTheRecordProfile(otr_profile_id);
}

jboolean JNI_Profile_HasPrimaryOTRProfile(JNIEnv* env, jlong ptr) {
  Profile* self = reinterpret_cast<Profile*>(ptr);
  return self->HasPrimaryOTRProfile();
}

ScopedJavaLocalRef<jobject> JNI_Profile_GetProfileKey(JNIEnv* env, jlong ptr) {
  Profile* self = reinterpret_cast<Profile*>(ptr);
  ProfileKeyAndroid* profile_key =
      self->GetProfileKey()->GetProfileKeyAndroid();
  DCHECK(profile_key);
  return profile_key->GetJavaObject();
}

jboolean JNI_Profile_IsChild(JNIEnv* env, jlong ptr) {
  Profile* self = reinterpret_cast<Profile*>(ptr);
  return self->IsChild();
}

void JNI_Profile_Wipe(JNIEnv* env, jlong ptr) {
  Profile* self = reinterpret_cast<Profile*>(ptr);
  self->Wipe();
}

ScopedJavaLocalRef<jobject> JNI_Profile_FromWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents) {
    return nullptr;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }
  return profile->GetJavaObject();
}
