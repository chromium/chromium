// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/android/sync_service_android_bridge.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/android/jni_headers/SyncServiceImpl_jni.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

// Native callback for the JNI GetAllNodes method. When
// SyncService::GetAllNodesForDebugging() completes, this method is called and
// the results are sent to the Java callback.
void NativeGetAllNodesCallback(
    JNIEnv* env,
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    base::Value::List result) {
  std::string json_string;
  if (!base::JSONWriter::Write(result, &json_string)) {
    DVLOG(1) << "Writing as JSON failed. Passing empty string to Java code.";
    json_string = std::string();
  }

  Java_SyncServiceImpl_onGetAllNodesResult(
      env, callback, ConvertUTF8ToJavaString(env, json_string));
}

ScopedJavaLocalRef<jintArray> ModelTypeSetToJavaIntArray(
    JNIEnv* env,
    syncer::ModelTypeSet types) {
  std::vector<int> type_vector;
  for (syncer::ModelType type : types) {
    type_vector.push_back(type);
  }
  return base::android::ToJavaIntArray(env, type_vector);
}

ScopedJavaLocalRef<jintArray> UserSelectableTypeSetToJavaIntArray(
    JNIEnv* env,
    syncer::UserSelectableTypeSet types) {
  std::vector<int> type_vector;
  for (syncer::UserSelectableType type : types) {
    type_vector.push_back(static_cast<int>(type));
  }
  return base::android::ToJavaIntArray(env, type_vector);
}

}  // namespace

SyncServiceAndroidBridge::SyncServiceAndroidBridge(
    JNIEnv* env,
    syncer::SyncService* native_sync_service,
    jobject java_sync_service)
    : native_sync_service_(native_sync_service),
      java_sync_service_(env, java_sync_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(native_sync_service_);
  native_sync_service_->AddObserver(this);
}

SyncServiceAndroidBridge::~SyncServiceAndroidBridge() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->RemoveObserver(this);
}

void SyncServiceAndroidBridge::OnStateChanged(syncer::SyncService* sync) {
  // Notify the java world that our sync state has changed.
  JNIEnv* env = AttachCurrentThread();
  Java_SyncServiceImpl_syncStateChanged(env, java_sync_service_.get(env));
}

void SyncServiceAndroidBridge::SetSyncRequested(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->SetSyncFeatureRequested();
}

jboolean SyncServiceAndroidBridge::CanSyncFeatureStart(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->CanSyncFeatureStart();
}

jboolean SyncServiceAndroidBridge::IsSyncFeatureEnabled(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->IsSyncFeatureEnabled();
}

jboolean SyncServiceAndroidBridge::IsSyncFeatureActive(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->IsSyncFeatureActive();
}

jboolean SyncServiceAndroidBridge::IsSyncDisabledByEnterprisePolicy(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

jboolean SyncServiceAndroidBridge::IsEngineInitialized(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->IsEngineInitialized();
}

jboolean SyncServiceAndroidBridge::IsTransportStateActive(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetTransportState() ==
         syncer::SyncService::TransportState::ACTIVE;
}

void SyncServiceAndroidBridge::SetSetupInProgress(JNIEnv* env,
                                                  jboolean in_progress) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!in_progress) {
    sync_blocker_.reset();
    return;
  }

  if (!sync_blocker_) {
    sync_blocker_ = native_sync_service_->GetSetupInProgressHandle();
  }
}

jboolean SyncServiceAndroidBridge::IsInitialSyncFeatureSetupComplete(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->IsInitialSyncFeatureSetupComplete();
}

void SyncServiceAndroidBridge::SetInitialSyncFeatureSetupComplete(JNIEnv* env,
                                                                  jint source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      static_cast<syncer::SyncFirstSetupCompleteSource>(source));
}

ScopedJavaLocalRef<jintArray> SyncServiceAndroidBridge::GetActiveDataTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ModelTypeSetToJavaIntArray(env,
                                    native_sync_service_->GetActiveDataTypes());
}

ScopedJavaLocalRef<jintArray> SyncServiceAndroidBridge::GetSelectedTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  syncer::UserSelectableTypeSet user_selectable_types;
  user_selectable_types =
      native_sync_service_->GetUserSettings()->GetSelectedTypes();
  return UserSelectableTypeSetToJavaIntArray(env, user_selectable_types);
}

jboolean SyncServiceAndroidBridge::IsTypeManagedByPolicy(JNIEnv* env,
                                                         jint type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK_GE(type, static_cast<int>(syncer::UserSelectableType::kFirstType));
  CHECK_LE(type, static_cast<int>(syncer::UserSelectableType::kLastType));
  return native_sync_service_->GetUserSettings()->IsTypeManagedByPolicy(
      static_cast<syncer::UserSelectableType>(type));
}

void SyncServiceAndroidBridge::SetSelectedTypes(
    JNIEnv* env,
    jboolean sync_everything,
    const JavaParamRef<jintArray>& user_selectable_type_array) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<int> types_vector;
  base::android::JavaIntArrayToIntVector(env, user_selectable_type_array,
                                         &types_vector);

  syncer::UserSelectableTypeSet user_selectable_types;
  for (int type : types_vector) {
    CHECK_GE(type, static_cast<int>(syncer::UserSelectableType::kFirstType));
    CHECK_LE(type, static_cast<int>(syncer::UserSelectableType::kLastType));
    user_selectable_types.Put(static_cast<syncer::UserSelectableType>(type));
  }

  native_sync_service_->GetUserSettings()->SetSelectedTypes(
      sync_everything, user_selectable_types);
}

jboolean SyncServiceAndroidBridge::IsCustomPassphraseAllowed(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsCustomPassphraseAllowed();
}

jboolean SyncServiceAndroidBridge::IsEncryptEverythingEnabled(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsEncryptEverythingEnabled();
}

jboolean SyncServiceAndroidBridge::IsPassphraseRequiredForPreferredDataTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->IsPassphraseRequiredForPreferredDataTypes();
}

jboolean SyncServiceAndroidBridge::IsTrustedVaultKeyRequired(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsTrustedVaultKeyRequired();
}

jboolean
SyncServiceAndroidBridge::IsTrustedVaultKeyRequiredForPreferredDataTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->IsTrustedVaultKeyRequiredForPreferredDataTypes();
}

jboolean SyncServiceAndroidBridge::IsTrustedVaultRecoverabilityDegraded(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->IsTrustedVaultRecoverabilityDegraded();
}

jboolean SyncServiceAndroidBridge::IsUsingExplicitPassphrase(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsUsingExplicitPassphrase();
}

jint SyncServiceAndroidBridge::GetPassphraseType(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return static_cast<unsigned>(
      native_sync_service_->GetUserSettings()->GetPassphraseType());
}

void SyncServiceAndroidBridge::SetEncryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->GetUserSettings()->SetEncryptionPassphrase(
      ConvertJavaStringToUTF8(env, passphrase));
}

jboolean SyncServiceAndroidBridge::SetDecryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->SetDecryptionPassphrase(
      ConvertJavaStringToUTF8(env, passphrase));
}

jlong SyncServiceAndroidBridge::GetExplicitPassphraseTime(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->GetExplicitPassphraseTime()
      .ToJavaTime();
}

void SyncServiceAndroidBridge::GetAllNodes(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);
  native_sync_service_->GetAllNodesForDebugging(
      base::BindOnce(&NativeGetAllNodesCallback, env, java_callback));
}

jint SyncServiceAndroidBridge::GetAuthError(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetAuthError().state();
}

jboolean SyncServiceAndroidBridge::HasUnrecoverableError(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->HasUnrecoverableError();
}

jboolean SyncServiceAndroidBridge::RequiresClientUpgrade(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->RequiresClientUpgrade();
}

base::android::ScopedJavaLocalRef<jobject>
SyncServiceAndroidBridge::GetAccountInfo(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CoreAccountInfo account_info = native_sync_service_->GetAccountInfo();
  return account_info.IsEmpty()
             ? nullptr
             : ConvertToJavaCoreAccountInfo(env, account_info);
}

jboolean SyncServiceAndroidBridge::HasSyncConsent(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->HasSyncConsent();
}

jboolean
SyncServiceAndroidBridge::IsPassphrasePromptMutedForCurrentProductVersion(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->IsPassphrasePromptMutedForCurrentProductVersion();
}

void SyncServiceAndroidBridge::
    MarkPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->GetUserSettings()
      ->MarkPassphrasePromptMutedForCurrentProductVersion();
}

jboolean SyncServiceAndroidBridge::HasKeepEverythingSynced(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsSyncEverythingEnabled();
}

jboolean SyncServiceAndroidBridge::ShouldOfferTrustedVaultOptIn(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return syncer::ShouldOfferTrustedVaultOptIn(native_sync_service_);
}

void SyncServiceAndroidBridge::TriggerRefresh(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->TriggerRefresh(syncer::ModelTypeSet::All());
}

jlong SyncServiceAndroidBridge::GetLastSyncedTimeForDebugging(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time last_sync_time =
      native_sync_service_->GetLastSyncedTimeForDebugging();
  return static_cast<jlong>(
      (last_sync_time - base::Time::UnixEpoch()).InMicroseconds());
}

static jlong JNI_SyncServiceImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_sync_service) {
  DCHECK(g_browser_process && g_browser_process->profile_manager());

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(ProfileManager::GetLastUsedProfile());
  if (!sync_service) {
    return 0;
  }

  // Owned by the caller.
  return reinterpret_cast<intptr_t>(
      new SyncServiceAndroidBridge(env, sync_service, java_sync_service));
}
