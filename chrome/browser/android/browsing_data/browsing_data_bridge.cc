// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/android/chrome_jni_headers/BrowsingDataBridge_jni.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;
using content::BrowsingDataRemover;

namespace {

void OnBrowsingDataRemoverDone(
    JavaObjectWeakGlobalRef weak_chrome_native_preferences) {
  JNIEnv* env = AttachCurrentThread();
  auto java_obj = weak_chrome_native_preferences.get(env);
  if (java_obj.is_null())
    return;

  Java_BrowsingDataBridge_browsingDataCleared(env, java_obj);
}

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

browsing_data::ClearBrowsingDataTab ToTabEnum(jint clear_browsing_data_tab) {
  DCHECK_GE(clear_browsing_data_tab, 0);
  DCHECK_LT(clear_browsing_data_tab,
            static_cast<int>(browsing_data::ClearBrowsingDataTab::NUM_TYPES));

  return static_cast<browsing_data::ClearBrowsingDataTab>(
      clear_browsing_data_tab);
}

}  // namespace

static void JNI_BrowsingDataBridge_ClearBrowsingData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jintArray>& data_types,
    jint time_period,
    const JavaParamRef<jobjectArray>& jexcluding_domains,
    const JavaParamRef<jintArray>& jexcluding_domain_reasons,
    const JavaParamRef<jobjectArray>& jignoring_domains,
    const JavaParamRef<jintArray>& jignoring_domain_reasons) {
  TRACE_EVENT0("browsing_data", "BrowsingDataBridge_ClearBrowsingData");

  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  BrowsingDataRemover* browsing_data_remover =
      content::BrowserContext::GetBrowsingDataRemover(profile);

  std::vector<int> data_types_vector;
  base::android::JavaIntArrayToIntVector(env, data_types, &data_types_vector);

  int remove_mask = 0;
  for (const int data_type : data_types_vector) {
    switch (static_cast<browsing_data::BrowsingDataType>(data_type)) {
      case browsing_data::BrowsingDataType::HISTORY:
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
        break;
      case browsing_data::BrowsingDataType::CACHE:
        remove_mask |= BrowsingDataRemover::DATA_TYPE_CACHE;
        break;
      case browsing_data::BrowsingDataType::COOKIES:
        remove_mask |= BrowsingDataRemover::DATA_TYPE_COOKIES;
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA;
        remove_mask |= BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES;
        break;
      case browsing_data::BrowsingDataType::PASSWORDS:
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;
        break;
      case browsing_data::BrowsingDataType::FORM_DATA:
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA;
        break;
      case browsing_data::BrowsingDataType::BOOKMARKS:
        // Bookmarks are deleted separately on the Java side.
        NOTREACHED();
        break;
      case browsing_data::BrowsingDataType::SITE_SETTINGS:
        remove_mask |=
            ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS;
        break;
      case browsing_data::BrowsingDataType::DOWNLOADS:
      case browsing_data::BrowsingDataType::HOSTED_APPS_DATA:
        // Only implemented on Desktop.
        NOTREACHED();
        FALLTHROUGH;
      case browsing_data::BrowsingDataType::NUM_TYPES:
        NOTREACHED();
    }
  }
  std::vector<std::string> excluding_domains;
  std::vector<int32_t> excluding_domain_reasons;
  std::vector<std::string> ignoring_domains;
  std::vector<int32_t> ignoring_domain_reasons;
  base::android::AppendJavaStringArrayToStringVector(env, jexcluding_domains,
                                                     &excluding_domains);
  base::android::JavaIntArrayToIntVector(env, jexcluding_domain_reasons,
                                         &excluding_domain_reasons);
  base::android::AppendJavaStringArrayToStringVector(env, jignoring_domains,
                                                     &ignoring_domains);
  base::android::JavaIntArrayToIntVector(env, jignoring_domain_reasons,
                                         &ignoring_domain_reasons);
  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder(
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::BLACKLIST));
  for (const std::string& domain : excluding_domains) {
    filter_builder->AddRegisterableDomain(domain);
  }

  if (!excluding_domains.empty() || !ignoring_domains.empty()) {
    ImportantSitesUtil::RecordBlacklistedAndIgnoredImportantSites(
        profile, excluding_domains, excluding_domain_reasons, ignoring_domains,
        ignoring_domain_reasons);
  }

  base::OnceClosure callback = base::BindOnce(
      &OnBrowsingDataRemoverDone, JavaObjectWeakGlobalRef(env, obj));

  browsing_data::TimePeriod period =
      static_cast<browsing_data::TimePeriod>(time_period);

  browsing_data_important_sites_util::Remove(
      remove_mask, BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, period,
      std::move(filter_builder), browsing_data_remover, std::move(callback));
}

static void EnableDialogAboutOtherFormsOfBrowsingHistory(
    const JavaRef<jobject>& listener,
    bool enabled) {
  JNIEnv* env = AttachCurrentThread();
  if (!enabled)
    return;
  Java_OtherFormsOfBrowsingHistoryListener_enableDialogAboutOtherFormsOfBrowsingHistory(
      env, listener);
}

static void JNI_BrowsingDataBridge_RequestInfoAboutOtherFormsOfBrowsingHistory(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& listener) {
  TRACE_EVENT0(
      "browsing_data",
      "BrowsingDataBridge_RequestInfoAboutOtherFormsOfBrowsingHistory");
  // The one-time notice in the dialog.
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
      ProfileSyncServiceFactory::GetForProfile(profile),
      WebHistoryServiceFactory::GetForProfile(profile), chrome::GetChannel(),
      base::Bind(&EnableDialogAboutOtherFormsOfBrowsingHistory,
                 ScopedJavaGlobalRef<jobject>(env, listener)));
}

static void JNI_BrowsingDataBridge_FetchImportantSites(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& java_callback) {
  TRACE_EVENT0("browsing_data", "BrowsingDataBridge_FetchImportantSites");
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  std::vector<ImportantSitesUtil::ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(
          profile, ImportantSitesUtil::kMaxImportantSites);
  bool dialog_disabled = ImportantSitesUtil::IsDialogDisabled(profile);

  std::vector<std::string> important_domains;
  std::vector<int32_t> important_domain_reasons;
  std::vector<std::string> important_domain_examples;
  for (const ImportantSitesUtil::ImportantDomainInfo& info : important_sites) {
    important_domains.push_back(info.registerable_domain);
    important_domain_reasons.push_back(info.reason_bitfield);
    important_domain_examples.push_back(info.example_origin.spec());
  }

  ScopedJavaLocalRef<jobjectArray> java_domains =
      base::android::ToJavaArrayOfStrings(env, important_domains);
  ScopedJavaLocalRef<jintArray> java_reasons =
      base::android::ToJavaIntArray(env, important_domain_reasons);
  ScopedJavaLocalRef<jobjectArray> java_origins =
      base::android::ToJavaArrayOfStrings(env, important_domain_examples);

  Java_ImportantSitesCallback_onImportantRegisterableDomainsReady(
      env, java_callback, java_domains, java_origins, java_reasons,
      dialog_disabled);
}

// This value should not change during a sessions, as it's used for UMA metrics.
static jint JNI_BrowsingDataBridge_GetMaxImportantSites(JNIEnv* env) {
  return ImportantSitesUtil::kMaxImportantSites;
}

static void JNI_BrowsingDataBridge_MarkOriginAsImportantForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jorigin) {
  GURL origin(base::android::ConvertJavaStringToUTF8(jorigin));
  CHECK(origin.is_valid());
  ImportantSitesUtil::MarkOriginAsImportantForTesting(
      ProfileAndroid::FromProfileAndroid(jprofile), origin);
}

static jboolean JNI_BrowsingDataBridge_GetBrowsingDataDeletionPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint data_type,
    jint clear_browsing_data_tab) {
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type,
            static_cast<int>(browsing_data::BrowsingDataType::NUM_TYPES));

  // If there is no corresponding preference for this |data_type|, pretend
  // that it's set to false.
  // TODO(msramek): Consider defining native-side preferences for all Java UI
  // data types for consistency.
  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type),
          ToTabEnum(clear_browsing_data_tab), &pref)) {
    return false;
  }

  return GetPrefService()->GetBoolean(pref);
}

static void JNI_BrowsingDataBridge_SetBrowsingDataDeletionPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint data_type,
    jint clear_browsing_data_tab,
    jboolean value) {
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type,
            static_cast<int>(browsing_data::BrowsingDataType::NUM_TYPES));

  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type),
          ToTabEnum(clear_browsing_data_tab), &pref)) {
    return;
  }

  GetPrefService()->SetBoolean(pref, value);
}

static jint JNI_BrowsingDataBridge_GetBrowsingDataDeletionTimePeriod(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint clear_browsing_data_tab) {
  return GetPrefService()->GetInteger(
      browsing_data::GetTimePeriodPreferenceName(
          ToTabEnum(clear_browsing_data_tab)));
}

static void JNI_BrowsingDataBridge_SetBrowsingDataDeletionTimePeriod(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint clear_browsing_data_tab,
    jint time_period) {
  DCHECK_GE(time_period, 0);
  DCHECK_LE(time_period,
            static_cast<int>(browsing_data::TimePeriod::TIME_PERIOD_LAST));
  const char* pref_name = browsing_data::GetTimePeriodPreferenceName(
      ToTabEnum(clear_browsing_data_tab));
  int previous_value = GetPrefService()->GetInteger(pref_name);
  if (time_period != previous_value) {
    browsing_data::RecordTimePeriodChange(
        static_cast<browsing_data::TimePeriod>(time_period));
    GetPrefService()->SetInteger(pref_name, time_period);
  }
}

static jint JNI_BrowsingDataBridge_GetLastClearBrowsingDataTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetInteger(
      browsing_data::prefs::kLastClearBrowsingDataTab);
}

static void JNI_BrowsingDataBridge_SetLastClearBrowsingDataTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint tab_index) {
  DCHECK_GE(tab_index, 0);
  DCHECK_LT(tab_index, 2);
  GetPrefService()->SetInteger(browsing_data::prefs::kLastClearBrowsingDataTab,
                               tab_index);
}
