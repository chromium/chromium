// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/android/profile_attributes_storage_android.h"

#include <optional>
#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_id.h"

// Must come after all headers that specialize `FromJniType()` / `ToJniType()`.
#include "chrome/browser/profiles/android/jni_headers/ProfileAttributesEntry_jni.h"
#include "chrome/browser/profiles/android/jni_headers/ProfileAttributesStorage_jni.h"

namespace {

ProfileAttributesStorage* GetStorage() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager ? &profile_manager->GetProfileAttributesStorage()
                         : nullptr;
}

ProfileAttributesEntry* GetEntry(const std::string& profile_path) {
  ProfileAttributesStorage* storage = GetStorage();
  return storage ? storage->GetProfileAttributesWithPath(
                       base::FilePath(profile_path))
                 : nullptr;
}

}  // namespace

ProfileAttributesStorageAndroid::ProfileAttributesStorageAndroid(
    ProfileAttributesStorage& storage) {
  java_object_.Reset(
      Java_ProfileAttributesStorage_create(jni_zero::AttachCurrentThread()));
  storage_observation_.Observe(&storage);
}

ProfileAttributesStorageAndroid::~ProfileAttributesStorageAndroid() {
  Java_ProfileAttributesStorage_destroy(jni_zero::AttachCurrentThread(),
                                        java_object_);
}

void ProfileAttributesStorageAndroid::OnProfileAdded(
    const base::FilePath& profile_path) {
  Java_ProfileAttributesStorage_onProfileAdded(
      jni_zero::AttachCurrentThread(), java_object_, profile_path.value());
}

void ProfileAttributesStorageAndroid::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const std::u16string& profile_name) {
  Java_ProfileAttributesStorage_onProfileWasRemoved(
      jni_zero::AttachCurrentThread(), java_object_, profile_path.value());
}

void ProfileAttributesStorageAndroid::OnProfileAuthInfoChanged(
    const base::FilePath& profile_path) {
  Java_ProfileAttributesStorage_onProfileAuthInfoChanged(
      jni_zero::AttachCurrentThread(), java_object_, profile_path.value());
}

void ProfileAttributesStorageAndroid::OnProfileIsOmittedChanged(
    const base::FilePath& profile_path) {
  Java_ProfileAttributesStorage_onProfileIsOmittedChanged(
      jni_zero::AttachCurrentThread(), java_object_, profile_path.value());
}

static std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
JNI_ProfileAttributesStorage_GetAllProfilesAttributes(JNIEnv* env) {
  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> entries;
  ProfileAttributesStorage* storage = GetStorage();
  if (!storage) {
    return entries;
  }
  for (const ProfileAttributesEntry* entry :
       storage->GetAllProfilesAttributes()) {
    entries.push_back(
        Java_ProfileAttributesEntry_Constructor(env, entry->GetPath().value()));
  }
  return entries;
}

static std::optional<GaiaId> JNI_ProfileAttributesEntry_GetGaiaId(
    JNIEnv* env,
    const std::string& profile_path) {
  ProfileAttributesEntry* entry = GetEntry(profile_path);
  if (!entry) {
    return std::nullopt;
  }
  GaiaId gaia_id = entry->GetGAIAId();
  if (gaia_id.empty()) {
    return std::nullopt;
  }
  return gaia_id;
}

static jboolean JNI_ProfileAttributesEntry_IsOmitted(
    JNIEnv* env,
    const std::string& profile_path) {
  ProfileAttributesEntry* entry = GetEntry(profile_path);
  return entry && entry->IsOmitted();
}

DEFINE_JNI(ProfileAttributesEntry)
DEFINE_JNI(ProfileAttributesStorage)
