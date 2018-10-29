// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_android.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/about_sync_util.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/engine/net/network_resources.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/browser/browser_thread.h"
#include "google/cacheinvalidation/types.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "jni/ProfileSyncService_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using browser_sync::ProfileSyncService;
using content::BrowserThread;
using unified_consent::UrlKeyedDataCollectionConsentHelper;

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

ProfileSyncServiceAndroid::ProfileSyncServiceAndroid(JNIEnv* env, jobject obj)
    : profile_(nullptr),
      sync_service_(nullptr),
      weak_java_profile_sync_service_(env, obj) {
  if (g_browser_process == nullptr ||
      g_browser_process->profile_manager() == nullptr) {
    NOTREACHED() << "Browser process or profile manager not initialized";
    return;
  }

  profile_ = ProfileManager::GetActiveUserProfile();
  if (profile_ == nullptr) {
    NOTREACHED() << "Sync Init: Profile not found.";
    return;
  }

  sync_prefs_ = std::make_unique<syncer::SyncPrefs>(profile_->GetPrefs());

  sync_service_ = ProfileSyncServiceFactory::GetForProfile(profile_);
}

bool ProfileSyncServiceAndroid::Init() {
  if (sync_service_) {
    sync_service_->AddObserver(this);
    return true;
  } else {
    return false;
  }
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

bool ProfileSyncServiceAndroid::IsSyncAllowedByAndroid() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  return Java_ProfileSyncService_isMasterSyncEnabled(
      env, weak_java_profile_sync_service_.get(env));
}

// Pure ProfileSyncService calls.

jboolean ProfileSyncServiceAndroid::IsSyncRequested(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Sync is considered requested if it's not explicitly disabled by the user.
  return !sync_service_->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE);
}

void ProfileSyncServiceAndroid::RequestStart(JNIEnv* env,
                                             const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->RequestStart();
}

void ProfileSyncServiceAndroid::RequestStop(JNIEnv* env,
                                            const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->RequestStop(ProfileSyncService::KEEP_DATA);
}

void ProfileSyncServiceAndroid::SetSyncAllowedByPlatform(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean allowed) {
  sync_service_->SetSyncAllowedByPlatform(allowed);
}

jboolean ProfileSyncServiceAndroid::IsSyncActive(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsSyncFeatureActive();
}

jboolean ProfileSyncServiceAndroid::IsEngineInitialized(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsEngineInitialized();
}

void ProfileSyncServiceAndroid::SetSetupInProgress(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
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

jboolean ProfileSyncServiceAndroid::IsFirstSetupComplete(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsFirstSetupComplete();
}

void ProfileSyncServiceAndroid::SetFirstSetupComplete(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->SetFirstSetupComplete();
}

ScopedJavaLocalRef<jintArray> ProfileSyncServiceAndroid::GetActiveDataTypes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  syncer::ModelTypeSet types = sync_service_->GetActiveDataTypes();
  return JNI_ProfileSyncService_ModelTypeSetToJavaIntArray(env, types);
}

ScopedJavaLocalRef<jintArray> ProfileSyncServiceAndroid::GetPreferredDataTypes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  syncer::ModelTypeSet types = sync_service_->GetPreferredDataTypes();
  return JNI_ProfileSyncService_ModelTypeSetToJavaIntArray(env, types);
}

void ProfileSyncServiceAndroid::SetPreferredDataTypes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean sync_everything,
    const JavaParamRef<jintArray>& model_type_array) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<int> types_vector;
  base::android::JavaIntArrayToIntVector(env, model_type_array, &types_vector);
  syncer::ModelTypeSet types;
  for (size_t i = 0; i < types_vector.size(); i++) {
    types.Put(static_cast<syncer::ModelType>(types_vector[i]));
  }
  sync_service_->OnUserChoseDatatypes(sync_everything, types);
}

jboolean ProfileSyncServiceAndroid::IsCryptographerReady(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  return sync_service_->IsCryptographerReady(&trans);
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingAllowed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsEncryptEverythingAllowed();
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsEncryptEverythingEnabled();
}

void ProfileSyncServiceAndroid::EnableEncryptEverything(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->EnableEncryptEverything();
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequiredForDecryption(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsPassphraseRequiredForDecryption();
}

jboolean ProfileSyncServiceAndroid::IsUsingSecondaryPassphrase(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsUsingSecondaryPassphrase();
}

ScopedJavaLocalRef<jbyteArray>
ProfileSyncServiceAndroid::GetCustomPassphraseKey(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  std::string key = sync_service_->GetCustomPassphraseKey();
  return base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(key.data()), key.size());
}

jint ProfileSyncServiceAndroid::GetPassphraseType(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return static_cast<unsigned>(sync_service_->GetPassphraseType());
}

void ProfileSyncServiceAndroid::SetEncryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  sync_service_->SetEncryptionPassphrase(key);
}

jboolean ProfileSyncServiceAndroid::SetDecryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  return sync_service_->SetDecryptionPassphrase(key);
}

jboolean ProfileSyncServiceAndroid::HasExplicitPassphraseTime(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  return !passphrase_time.is_null();
}

jlong ProfileSyncServiceAndroid::GetExplicitPassphraseTime(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  return passphrase_time.ToJavaTime();
}

void ProfileSyncServiceAndroid::FlushDirectory(JNIEnv* env,
                                               const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->FlushDirectory();
}

void ProfileSyncServiceAndroid::GetAllNodes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);

  base::Callback<void(std::unique_ptr<base::ListValue>)> native_callback =
      base::Bind(&NativeGetAllNodesCallback, java_callback);
  sync_service_->GetAllNodes(native_callback);
}

jint ProfileSyncServiceAndroid::GetAuthError(JNIEnv* env,
                                             const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetAuthError().state();
}

jboolean ProfileSyncServiceAndroid::HasUnrecoverableError(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->HasUnrecoverableError();
}

jboolean ProfileSyncServiceAndroid::IsUrlKeyedDataCollectionEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean personalized) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
      unified_consent_url_helper;
  if (personalized) {
    unified_consent_url_helper = UrlKeyedDataCollectionConsentHelper::
        NewPersonalizedDataCollectionConsentHelper(sync_service_);
  } else {
    PrefService* pref_service = profile_->GetPrefs();
    unified_consent_url_helper = UrlKeyedDataCollectionConsentHelper::
        NewAnonymizedDataCollectionConsentHelper(pref_service, sync_service_);
  }

  return unified_consent_url_helper->IsEnabled();
}

jint ProfileSyncServiceAndroid::GetProtocolErrorClientAction(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  syncer::SyncStatus status;
  sync_service_->QueryDetailedSyncStatus(&status);
  return status.sync_protocol_error.action;
}

// Pure SyncPrefs calls.

jboolean ProfileSyncServiceAndroid::IsPassphrasePrompted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return sync_prefs_->IsPassphrasePrompted();
}

void ProfileSyncServiceAndroid::SetPassphrasePrompted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean prompted) {
  sync_prefs_->SetPassphrasePrompted(prompted);
}

void ProfileSyncServiceAndroid::SetSyncSessionsId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& tag) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  std::string machine_tag = ConvertJavaStringToUTF8(env, tag);
  SessionSyncServiceFactory::GetForProfile(profile_)->SetSyncSessionsGUID(
      machine_tag);
}

jboolean ProfileSyncServiceAndroid::HasKeepEverythingSynced(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_prefs_->HasKeepEverythingSynced();
}

// UI string getters.

ScopedJavaLocalRef<jstring>
ProfileSyncServiceAndroid::GetSyncEnterGooglePassphraseBodyWithDateText(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  base::string16 passphrase_time_str =
      base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(
        IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyWithDateText(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  base::string16 passphrase_time_str =
      base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
ProfileSyncServiceAndroid::GetCurrentSignedInAccountText(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string& sync_username =
      sync_service_->GetAuthenticatedAccountInfo().email;
  return base::android::ConvertUTF16ToJavaString(
      env, l10n_util::GetStringFUTF16(IDS_SYNC_ACCOUNT_INFO,
                                      base::ASCIIToUTF16(sync_username)));
}

ScopedJavaLocalRef<jstring>
ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyText(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_SYNC_ENTER_PASSPHRASE_BODY));
}

// Functionality only available for testing purposes.

jlong ProfileSyncServiceAndroid::GetLastSyncedTimeForTest(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  // Use profile preferences here instead of SyncPrefs to avoid an extra
  // conversion, since SyncPrefs::GetLastSyncedTime() converts the stored value
  // to to base::Time.
  return static_cast<jlong>(
      profile_->GetPrefs()->GetInt64(syncer::prefs::kSyncLastSyncedTime));
}

void ProfileSyncServiceAndroid::OverrideNetworkResourcesForTest(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong network_resources) {
  syncer::NetworkResources* resources =
      reinterpret_cast<syncer::NetworkResources*>(network_resources);
  sync_service_->OverrideNetworkResourcesForTest(
      base::WrapUnique<syncer::NetworkResources>(resources));
}

// static
ProfileSyncServiceAndroid*
    ProfileSyncServiceAndroid::GetProfileSyncServiceAndroid() {
  return reinterpret_cast<ProfileSyncServiceAndroid*>(
      Java_ProfileSyncService_getProfileSyncServiceAndroid(
          AttachCurrentThread()));
}

static jlong JNI_ProfileSyncService_Init(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  ProfileSyncServiceAndroid* profile_sync_service_android =
      new ProfileSyncServiceAndroid(env, obj);
  if (profile_sync_service_android->Init()) {
    return reinterpret_cast<intptr_t>(profile_sync_service_android);
  } else {
    delete profile_sync_service_android;
    return 0;
  }
}
