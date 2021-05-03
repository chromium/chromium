// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_android.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/ProfileSyncService_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
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
// ProfileSyncService::GetAllNodes completes, this method is called and the
// results are sent to the Java callback.
void NativeGetAllNodesCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    std::unique_ptr<base::ListValue> result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json_string;
  if (!result.get() || !base::JSONWriter::Write(*result, &json_string)) {
    DVLOG(1) << "Writing as JSON failed. Passing empty string to Java code.";
    json_string = std::string();
  }

  ScopedJavaLocalRef<jstring> java_json_string =
      ConvertUTF8ToJavaString(env, json_string);
  Java_ProfileSyncService_onGetAllNodesResult(env, callback, java_json_string);
}

ScopedJavaLocalRef<jintArray> JNI_ProfileSyncService_ModelTypeSetToJavaIntArray(
    JNIEnv* env,
    syncer::ModelTypeSet types) {
  std::vector<int> type_vector;
  for (syncer::ModelType type : types) {
    type_vector.push_back(type);
  }
  return base::android::ToJavaIntArray(env, type_vector);
}

}  // namespace

ProfileSyncServiceAndroid::ProfileSyncServiceAndroid(
    JNIEnv* env,
    jobject java_profile_sync_service)
    : profile_(nullptr),
      sync_service_(nullptr),
      weak_java_profile_sync_service_(env, java_profile_sync_service) {
  if (g_browser_process == nullptr ||
      g_browser_process->profile_manager() == nullptr) {
    NOTREACHED() << "Browser process or profile manager not initialized";
    return;
  }

  profile_ = ProfileManager::GetLastUsedProfile();
  if (profile_ == nullptr) {
    NOTREACHED() << "Sync Init: Profile not found.";
    return;
  }

  sync_service_ =
      ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(profile_);
}

bool ProfileSyncServiceAndroid::Init() {
  if (sync_service_) {
    sync_service_->AddObserver(this);
    return true;
  }
  return false;
}

ProfileSyncServiceAndroid::~ProfileSyncServiceAndroid() {
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
  }
}

void ProfileSyncServiceAndroid::OnStateChanged(syncer::SyncService* sync) {
  // Notify the java world that our sync state has changed.
  JNIEnv* env = AttachCurrentThread();
  Java_ProfileSyncService_syncStateChanged(
      env, weak_java_profile_sync_service_.get(env));
}

// Pure ProfileSyncService calls.

jboolean ProfileSyncServiceAndroid::IsSyncRequested(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetUserSettings()->IsSyncRequested();
}

jboolean ProfileSyncServiceAndroid::CanSyncFeatureStart(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->CanSyncFeatureStart();
}

void ProfileSyncServiceAndroid::SetSyncRequested(JNIEnv* env,
                                                 jboolean requested) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->GetUserSettings()->SetSyncRequested(requested);
}

jboolean ProfileSyncServiceAndroid::IsSyncAllowedByPlatform(JNIEnv* env) {
  return !sync_service_->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_PLATFORM_OVERRIDE);
}

void ProfileSyncServiceAndroid::SetSyncAllowedByPlatform(
    JNIEnv* env,
    jboolean allowed) {
  sync_service_->SetSyncAllowedByPlatform(allowed);
}

jboolean ProfileSyncServiceAndroid::IsSyncFeatureActive(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsSyncFeatureActive();
}

jboolean ProfileSyncServiceAndroid::IsSyncDisabledByEnterprisePolicy(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

jboolean ProfileSyncServiceAndroid::IsEngineInitialized(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsEngineInitialized();
}

jboolean ProfileSyncServiceAndroid::IsTransportStateActive(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetTransportState() ==
         syncer::SyncService::TransportState::ACTIVE;
}

void ProfileSyncServiceAndroid::SetSetupInProgress(
    JNIEnv* env,
    jboolean in_progress) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (in_progress) {
    if (!sync_blocker_) {
      sync_blocker_ = sync_service_->GetSetupInProgressHandle();
    }
  } else {
    sync_blocker_.reset();
  }
}

jboolean ProfileSyncServiceAndroid::IsFirstSetupComplete(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetUserSettings()->IsFirstSetupComplete();
}

void ProfileSyncServiceAndroid::SetFirstSetupComplete(
    JNIEnv* env,
    jint source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->GetUserSettings()->SetFirstSetupComplete(
      static_cast<syncer::SyncFirstSetupCompleteSource>(source));
}

ScopedJavaLocalRef<jintArray> ProfileSyncServiceAndroid::GetActiveDataTypes(
    JNIEnv* env) {
  syncer::ModelTypeSet types = sync_service_->GetActiveDataTypes();
  return JNI_ProfileSyncService_ModelTypeSetToJavaIntArray(env, types);
}

ScopedJavaLocalRef<jintArray> ProfileSyncServiceAndroid::GetChosenDataTypes(
    JNIEnv* env) {
  // TODO(crbug/950874): introduce UserSelectableType in java code, then remove
  // workaround here and in SetChosenDataTypes().
  syncer::UserSelectableTypeSet types =
      sync_service_->GetUserSettings()->GetSelectedTypes();
  syncer::ModelTypeSet model_types;
  for (syncer::UserSelectableType type : types) {
    model_types.Put(syncer::UserSelectableTypeToCanonicalModelType(type));
  }
  return JNI_ProfileSyncService_ModelTypeSetToJavaIntArray(env, model_types);
}

ScopedJavaLocalRef<jintArray> ProfileSyncServiceAndroid::GetPreferredDataTypes(
    JNIEnv* env) {
  syncer::ModelTypeSet types = sync_service_->GetPreferredDataTypes();
  return JNI_ProfileSyncService_ModelTypeSetToJavaIntArray(env, types);
}

void ProfileSyncServiceAndroid::SetChosenDataTypes(
    JNIEnv* env,
    jboolean sync_everything,
    const JavaParamRef<jintArray>& model_type_array) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<int> types_vector;
  base::android::JavaIntArrayToIntVector(env, model_type_array, &types_vector);
  syncer::ModelTypeSet model_types;
  for (size_t i = 0; i < types_vector.size(); i++) {
    model_types.Put(static_cast<syncer::ModelType>(types_vector[i]));
  }
  syncer::UserSelectableTypeSet selected_types;
  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    if (model_types.Has(syncer::UserSelectableTypeToCanonicalModelType(type))) {
      selected_types.Put(type);
    }
  }
  sync_service_->GetUserSettings()->SetSelectedTypes(sync_everything,
                                                     selected_types);
}

jboolean ProfileSyncServiceAndroid::IsCustomPassphraseAllowed(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetUserSettings()->IsCustomPassphraseAllowed();
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingEnabled(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetUserSettings()->IsEncryptEverythingEnabled();
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequiredForPreferredDataTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetUserSettings()
      ->IsPassphraseRequiredForPreferredDataTypes();
}

jboolean ProfileSyncServiceAndroid::IsTrustedVaultKeyRequired(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetUserSettings()->IsTrustedVaultKeyRequired();
}

jboolean
ProfileSyncServiceAndroid::IsTrustedVaultKeyRequiredForPreferredDataTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetUserSettings()
      ->IsTrustedVaultKeyRequiredForPreferredDataTypes();
}

jboolean ProfileSyncServiceAndroid::IsUsingExplicitPassphrase(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetUserSettings()->IsUsingExplicitPassphrase();
}

jint ProfileSyncServiceAndroid::GetPassphraseType(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return static_cast<unsigned>(
      sync_service_->GetUserSettings()->GetPassphraseType());
}

void ProfileSyncServiceAndroid::SetEncryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  sync_service_->GetUserSettings()->SetEncryptionPassphrase(key);
}

jboolean ProfileSyncServiceAndroid::SetDecryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  return sync_service_->GetUserSettings()->SetDecryptionPassphrase(key);
}

jlong ProfileSyncServiceAndroid::GetExplicitPassphraseTime(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time =
      sync_service_->GetUserSettings()->GetExplicitPassphraseTime();
  return passphrase_time.ToJavaTime();
}

void ProfileSyncServiceAndroid::GetAllNodes(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback) {
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);
  sync_service_->GetAllNodesForDebugging(
      base::BindOnce(&NativeGetAllNodesCallback, java_callback));
}

jint ProfileSyncServiceAndroid::GetAuthError(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetAuthError().state();
}

jboolean ProfileSyncServiceAndroid::HasUnrecoverableError(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->HasUnrecoverableError();
}

jboolean ProfileSyncServiceAndroid::RequiresClientUpgrade(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->RequiresClientUpgrade();
}

void ProfileSyncServiceAndroid::SetDecoupledFromAndroidMasterSync(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->SetDecoupledFromAndroidMasterSync();
}

jboolean ProfileSyncServiceAndroid::GetDecoupledFromAndroidMasterSync(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetDecoupledFromAndroidMasterSync();
}

base::android::ScopedJavaLocalRef<jobject>
ProfileSyncServiceAndroid::GetAuthenticatedAccountInfo(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CoreAccountInfo account_info = sync_service_->GetAuthenticatedAccountInfo();
  return account_info.IsEmpty()
             ? nullptr
             : ConvertToJavaCoreAccountInfo(env, account_info);
}

jboolean ProfileSyncServiceAndroid::IsAuthenticatedAccountPrimary(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsAuthenticatedAccountPrimary();
}

jboolean
ProfileSyncServiceAndroid::IsPassphrasePromptMutedForCurrentProductVersion(
    JNIEnv* env) {
  return sync_service_->GetUserSettings()
      ->IsPassphrasePromptMutedForCurrentProductVersion();
}

void ProfileSyncServiceAndroid::
    MarkPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env) {
  sync_service_->GetUserSettings()
      ->MarkPassphrasePromptMutedForCurrentProductVersion();
}

jboolean ProfileSyncServiceAndroid::HasKeepEverythingSynced(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetUserSettings()->IsSyncEverythingEnabled();
}

void ProfileSyncServiceAndroid::RecordKeyRetrievalTrigger(
    JNIEnv* env,
    jint trigger) {
  syncer::RecordKeyRetrievalTrigger(
      static_cast<syncer::KeyRetrievalTriggerForUMA>(trigger));
}

// Functionality only available for testing purposes.

jlong ProfileSyncServiceAndroid::GetProfileSyncServiceForTest(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(sync_service_);
}

jlong ProfileSyncServiceAndroid::GetLastSyncedTimeForTest(JNIEnv* env) {
  base::Time last_sync_time = sync_service_->GetLastSyncedTimeForDebugging();
  return static_cast<jlong>(
      (last_sync_time - base::Time::UnixEpoch()).InMicroseconds());
}

void ProfileSyncServiceAndroid::OverrideNetworkForTest(
    const syncer::CreateHttpPostProviderFactory&
        create_http_post_provider_factory_cb) {
  sync_service_->OverrideNetworkForTest(create_http_post_provider_factory_cb);
}

void ProfileSyncServiceAndroid::TriggerRefresh(JNIEnv* env) {
  // Only allowed to trigger refresh/schedule nudges for protocol types, things
  // like PROXY_TABS are not allowed.
  sync_service_->TriggerRefresh(syncer::ProtocolTypes());
}

static jlong JNI_ProfileSyncService_Init(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  ProfileSyncServiceAndroid* profile_sync_service_android =
      new ProfileSyncServiceAndroid(env, obj);
  if (profile_sync_service_android->Init()) {
    return reinterpret_cast<intptr_t>(profile_sync_service_android);
  }
  delete profile_sync_service_android;
  return 0;
}
