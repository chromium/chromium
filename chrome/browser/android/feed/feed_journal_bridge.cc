// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_journal_bridge.h"

#include <jni.h>

#include <list>
#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/core/feed_journal_database.h"
#include "components/feed/core/feed_journal_mutation.h"
#include "jni/FeedJournalBridge_jni.h"

namespace feed {

using base::android::AttachCurrentThread;
using base::android::JavaByteArrayToByteVector;
using base::android::JavaRef;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaArrayOfStrings;

namespace {

void OnLoadJournalDone(
    base::android::ScopedJavaGlobalRef<jobject> success_callback,
    base::android::ScopedJavaGlobalRef<jobject> failure_callback,
    bool success,
    std::vector<std::string> entries) {
  JNIEnv* env = AttachCurrentThread();

  if (success) {
    RunObjectCallbackAndroid(success_callback,
                             ToJavaArrayOfByteArray(env, entries));
  } else {
    RunObjectCallbackAndroid(failure_callback, nullptr);
  }
}

void OnLoadJournalKeyDone(
    base::android::ScopedJavaGlobalRef<jobject> success_callback,
    base::android::ScopedJavaGlobalRef<jobject> failure_callback,
    bool success,
    std::vector<std::string> entries) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_entries =
      ToJavaArrayOfStrings(env, entries);

  if (success) {
    RunObjectCallbackAndroid(success_callback, j_entries);
  } else {
    RunObjectCallbackAndroid(failure_callback, nullptr);
  }
}

void OnStorageCheckExistingCallbackDone(
    ScopedJavaGlobalRef<jobject> success_callback,
    ScopedJavaGlobalRef<jobject> failure_callback,
    bool success,
    bool exists) {
  if (success) {
    RunBooleanCallbackAndroid(success_callback, exists);
  } else {
    RunObjectCallbackAndroid(failure_callback, nullptr);
  }
}

void OnStorageBooleanCallbackDone(ScopedJavaGlobalRef<jobject> callback,
                                  bool exists) {
  RunBooleanCallbackAndroid(callback, exists);
}

}  // namespace

static jlong JNI_FeedJournalBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  DCHECK(host_service);
  FeedJournalDatabase* feed_journal_database =
      host_service->GetJournalDatabase();
  DCHECK(feed_journal_database);
  FeedJournalBridge* native_journal_bridge =
      new FeedJournalBridge(feed_journal_database);
  return reinterpret_cast<intptr_t>(native_journal_bridge);
}

FeedJournalBridge::FeedJournalBridge(FeedJournalDatabase* feed_journal_database)
    : feed_journal_database_(feed_journal_database), weak_ptr_factory_(this) {
  DCHECK(feed_journal_database_);
}

FeedJournalBridge::~FeedJournalBridge() = default;

void FeedJournalBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

void FeedJournalBridge::LoadJournal(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_journal_name,
    const base::android::JavaRef<jobject>& j_success_callback,
    const base::android::JavaRef<jobject>& j_failure_callback) {
  std::string journal_name = ConvertJavaStringToUTF8(j_env, j_journal_name);
  ScopedJavaGlobalRef<jobject> success_callback(j_success_callback);
  ScopedJavaGlobalRef<jobject> failure_callback(j_failure_callback);

  feed_journal_database_->LoadJournal(
      journal_name,
      base::BindOnce(&OnLoadJournalDone, success_callback, failure_callback));
}

void FeedJournalBridge::CommitJournalMutation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_callback) {
  DCHECK(journal_mutation_);
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  feed_journal_database_->CommitJournalMutation(
      std::move(journal_mutation_),
      base::BindOnce(&OnStorageBooleanCallbackDone, callback));
}

void FeedJournalBridge::DoesJournalExist(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_journal_name,
    const base::android::JavaRef<jobject>& j_success_callback,
    const base::android::JavaRef<jobject>& j_failure_callback) {
  std::string journal_name = ConvertJavaStringToUTF8(j_env, j_journal_name);
  ScopedJavaGlobalRef<jobject> success_callback(j_success_callback);
  ScopedJavaGlobalRef<jobject> failure_callback(j_failure_callback);

  feed_journal_database_->DoesJournalExist(
      journal_name, base::BindOnce(&OnStorageCheckExistingCallbackDone,
                                   success_callback, failure_callback));
}

void FeedJournalBridge::LoadAllJournalKeys(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_success_callback,
    const base::android::JavaRef<jobject>& j_failure_callback) {
  ScopedJavaGlobalRef<jobject> success_callback(j_success_callback);
  ScopedJavaGlobalRef<jobject> failure_callback(j_failure_callback);

  feed_journal_database_->LoadAllJournalKeys(base::BindOnce(
      &OnLoadJournalKeyDone, success_callback, failure_callback));
}

void FeedJournalBridge::DeleteAllJournals(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  feed_journal_database_->DeleteAllJournals(
      base::BindOnce(&OnStorageBooleanCallbackDone, callback));
}

void FeedJournalBridge::StartJournalMutation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_journal_name) {
  DCHECK(!journal_mutation_);
  std::string journal_name = ConvertJavaStringToUTF8(j_env, j_journal_name);
  journal_mutation_ =
      std::make_unique<JournalMutation>(std::move(journal_name));
}

void FeedJournalBridge::DeleteJournalMutation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this) {
  DCHECK(journal_mutation_);
  journal_mutation_.reset();
}

void FeedJournalBridge::AddAppendOperation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jbyteArray>& j_value) {
  DCHECK(journal_mutation_);
  std::vector<uint8_t> bytes_vector;
  JavaByteArrayToByteVector(j_env, j_value, &bytes_vector);
  journal_mutation_->AddAppendOperation(
      std::string(bytes_vector.begin(), bytes_vector.end()));
}

void FeedJournalBridge::AddCopyOperation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_to_journal_name) {
  DCHECK(journal_mutation_);
  std::string to_journal_name =
      ConvertJavaStringToUTF8(j_env, j_to_journal_name);
  journal_mutation_->AddCopyOperation(std::move(to_journal_name));
}

void FeedJournalBridge::AddDeleteOperation(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this) {
  DCHECK(journal_mutation_);
  journal_mutation_->AddDeleteOperation();
}

}  // namespace feed
