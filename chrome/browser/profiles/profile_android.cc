// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/android/jni_headers/Profile_jni.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_key_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {
const char kProfileAndroidKey[] = "profile_android";
}  // namespace

// static
ProfileAndroid* ProfileAndroid::FromProfile(Profile* profile) {
  if (!profile)
    return NULL;

  ProfileAndroid* profile_android = static_cast<ProfileAndroid*>(
      profile->GetUserData(kProfileAndroidKey));
  if (!profile_android) {
    profile_android = new ProfileAndroid(profile);
    profile->SetUserData(kProfileAndroidKey, base::WrapUnique(profile_android));
  }
  return profile_android;
}

// static
Profile* ProfileAndroid::FromProfileAndroid(const JavaRef<jobject>& obj) {
  if (obj.is_null())
    return NULL;

  ProfileAndroid* profile_android = reinterpret_cast<ProfileAndroid*>(
      Java_Profile_getNativePointer(AttachCurrentThread(), obj));
  if (!profile_android)
    return NULL;
  return profile_android->profile_;
}

// static
ScopedJavaLocalRef<jobject> ProfileAndroid::GetLastUsedRegularProfile(
    JNIEnv* env) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == NULL) {
    NOTREACHED() << "Profile not found.";
    return ScopedJavaLocalRef<jobject>();
  }

  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(profile);
  if (profile_android == NULL) {
    NOTREACHED() << "ProfileAndroid not found.";
    return ScopedJavaLocalRef<jobject>();
  }

  return ScopedJavaLocalRef<jobject>(profile_android->obj_);
}

void ProfileAndroid::DestroyWhenAppropriate(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  CHECK(profile_->IsOffTheRecord())
      << "Only OTR profiles can be destroyed from Java as regular profiles are "
         "owned by the C++ ProfileManager.";
  // Don't delete the Profile directly because the corresponding
  // RenderViewHost might not be deleted yet.
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(profile_);
}

base::android::ScopedJavaLocalRef<jobject> ProfileAndroid::GetOriginalProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ProfileAndroid* original_profile = ProfileAndroid::FromProfile(
      profile_->GetOriginalProfile());
  DCHECK(original_profile);
  return original_profile->GetJavaObject();
}

base::android::ScopedJavaLocalRef<jobject>
ProfileAndroid::GetOffTheRecordProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_otr_profile_id,
    const jboolean j_create_if_needed) {
  Profile::OTRProfileID otr_profile_id =
      Profile::OTRProfileID::ConvertFromJavaOTRProfileID(env, j_otr_profile_id);
  ProfileAndroid* otr_profile = ProfileAndroid::FromProfile(
      profile_->GetOffTheRecordProfile(otr_profile_id, j_create_if_needed));
  if (!j_create_if_needed && !otr_profile)
    return nullptr;
  DCHECK(otr_profile);
  return otr_profile->GetJavaObject();
}

base::android::ScopedJavaLocalRef<jobject> ProfileAndroid::GetPrimaryOTRProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const jboolean j_create_if_needed) {
  ProfileAndroid* otr_profile = ProfileAndroid::FromProfile(
      profile_->GetPrimaryOTRProfile(j_create_if_needed));
  if (!j_create_if_needed && !otr_profile)
    return nullptr;
  DCHECK(otr_profile);
  return otr_profile->GetJavaObject();
}

base::android::ScopedJavaLocalRef<jobject> ProfileAndroid::GetOTRProfileID(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return profile_->GetOTRProfileID().ConvertToJavaOTRProfileID(env);
}

jboolean ProfileAndroid::HasOffTheRecordProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_otr_profile_id) {
  Profile::OTRProfileID otr_profile_id =
      Profile::OTRProfileID::ConvertFromJavaOTRProfileID(env, j_otr_profile_id);
  return profile_->HasOffTheRecordProfile(otr_profile_id);
}

jboolean ProfileAndroid::HasPrimaryOTRProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return profile_->HasPrimaryOTRProfile();
}

jboolean ProfileAndroid::IsPrimaryOTRProfile(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  return profile_->IsPrimaryOTRProfile();
}

base::android::ScopedJavaLocalRef<jobject> ProfileAndroid::GetProfileKey(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ProfileKeyAndroid* profile_key =
      profile_->GetProfileKey()->GetProfileKeyAndroid();
  DCHECK(profile_key);
  return profile_key->GetJavaObject();
}

jboolean ProfileAndroid::IsOffTheRecord(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return profile_->IsOffTheRecord();
}

jboolean ProfileAndroid::IsChild(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return profile_->IsChild();
}

void ProfileAndroid::Wipe(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  profile_->Wipe();
}

jlong ProfileAndroid::GetBrowserContextPointer(JNIEnv* env) {
  return reinterpret_cast<jlong>(
      static_cast<content::BrowserContext*>(profile_));
}

// static
ScopedJavaLocalRef<jobject> JNI_Profile_GetLastUsedRegularProfile(JNIEnv* env) {
  return ProfileAndroid::GetLastUsedRegularProfile(env);
}

// static
ScopedJavaLocalRef<jobject> JNI_Profile_FromWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return ScopedJavaLocalRef<jobject>();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return ScopedJavaLocalRef<jobject>();
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(profile);
  if (!profile_android)
    return ScopedJavaLocalRef<jobject>();
  return profile_android->GetJavaObject();
}

ProfileAndroid::ProfileAndroid(Profile* profile)
    : profile_(profile) {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jprofile =
      Java_Profile_create(env, reinterpret_cast<intptr_t>(this));
  obj_.Reset(env, jprofile.obj());
}

ProfileAndroid::~ProfileAndroid() {
  Java_Profile_onNativeDestroyed(AttachCurrentThread(), obj_);
}

base::android::ScopedJavaLocalRef<jobject> ProfileAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}
