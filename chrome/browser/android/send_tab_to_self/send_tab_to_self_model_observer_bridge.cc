// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/send_tab_to_self_model_observer_bridge.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/SendTabToSelfModelObserverBridge_jni.h"
#include "chrome/browser/android/send_tab_to_self/send_tab_to_self_entry_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace send_tab_to_self {

SendTabToSelfModelObserverBridge::SendTabToSelfModelObserverBridge(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jobject>& j_profile)
    : weak_java_ref_(env, obj), send_tab_to_self_model_(nullptr) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  send_tab_to_self_model_ = SendTabToSelfSyncServiceFactory::GetInstance()
                                ->GetForProfile(profile)
                                ->GetSendTabToSelfModel();
  send_tab_to_self_model_->AddObserver(this);
}

static jlong JNI_SendTabToSelfModelObserverBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_profile) {
  SendTabToSelfModelObserverBridge* bridge =
      new SendTabToSelfModelObserverBridge(env, obj, j_profile);
  return reinterpret_cast<intptr_t>(bridge);
}

void SendTabToSelfModelObserverBridge::Destroy(JNIEnv*) {
  delete this;
}

SendTabToSelfModelObserverBridge::~SendTabToSelfModelObserverBridge() {
  send_tab_to_self_model_->RemoveObserver(this);
}

void SendTabToSelfModelObserverBridge::SendTabToSelfModelLoaded() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_SendTabToSelfModelObserverBridge_modelLoaded(env, obj);
}

void SendTabToSelfModelObserverBridge::EntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  ScopedJavaLocalRef<jobject> j_entry_list =
      Java_SendTabToSelfModelObserverBridge_createEmptyJavaEntryList(env, obj);

  for (std::vector<const SendTabToSelfEntry*>::const_iterator it =
           new_entries.begin();
       it != new_entries.end(); ++it) {
    Java_SendTabToSelfModelObserverBridge_addToEntryList(
        env, obj, j_entry_list, CreateJavaSendTabToSelfEntry(env, *it));
  }
  Java_SendTabToSelfModelObserverBridge_entriesAddedRemotely(env, obj,
                                                             j_entry_list);
}

void SendTabToSelfModelObserverBridge::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  ScopedJavaLocalRef<jobject> j_guid_list =
      Java_SendTabToSelfModelObserverBridge_createEmptyJavaGuidList(env, obj);

  for (std::vector<const std::string>::iterator it = guids.begin();
       it != guids.end(); ++it) {
    ScopedJavaLocalRef<jstring> j_guid = ConvertUTF8ToJavaString(env, *it);
    Java_SendTabToSelfModelObserverBridge_addToGuidList(env, obj, j_guid_list,
                                                        j_guid);
  }
  Java_SendTabToSelfModelObserverBridge_entriesRemovedRemotely(env, obj,
                                                               j_guid_list);
}
}  // namespace send_tab_to_self
