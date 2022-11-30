// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/content_creation/notes/internal/android/jni_headers/NoteServiceFactory_jni.h"
#include "chrome/browser/content_creation/notes/internal/note_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/content_creation/notes/android/note_service_bridge.h"

// Takes a Java Profile and returns a Java NoteService.
static base::android::ScopedJavaLocalRef<jobject>
JNI_NoteServiceFactory_GetForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);

  // Return null if there is no reasonable context for the provided Java
  // profile.
  if (profile == nullptr)
    return base::android::ScopedJavaLocalRef<jobject>();

  content_creation::NoteService* note_service =
      content_creation::NoteServiceFactory::GetForProfile(profile);
  return content_creation::NoteServiceBridge::GetBridgeForNoteService(
      note_service);
}
