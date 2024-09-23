// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/browsing_data/content/android/browsing_data_model_android.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/BrowsingDataBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowsingDataRemover;

namespace {

void OnBrowsingDataRemoverDone(const ScopedJavaGlobalRef<jobject>& callback,
                               uint64_t failed_data_types) {
  if (!callback) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_OnClearBrowsingDataListener_onBrowsingDataCleared(env, callback);
}

PrefService* GetPrefService(const JavaParamRef<jobject>& jprofile) {
  Profile* profile = Profile::FromJavaObject(jprofile);
  return profile->GetOriginalProfile()->GetPrefs();
}

browsing_data::ClearBrowsingDataTab ToTabEnum(jint clear_browsing_data_tab) {
  DCHECK_GE(clear_browsing_data_tab, 0);
  DCHECK_LE(clear_browsing_data_tab,
            static_cast<int>(browsing_data::ClearBrowsingDataTab::MAX_VALUE));

  return static_cast<browsing_data::ClearBrowsingDataTab>(
      clear_browsing_data_tab);
}

void OnBrowsingDataModelBuilt(JNIEnv* env,
                              const ScopedJavaGlobalRef<jobject>& java_callback,
                              std::unique_ptr<BrowsingDataModel> model) {
  Java_BrowsingDataBridge_onBrowsingDataModelBuilt(
      env, java_callback,
      reinterpret_cast<intptr_t>(
          new BrowsingDataModelAndroid(std::move(model))));
}

}  // namespace

static void JNI_BrowsingDataBridge_ClearBrowsingData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& jcallback,
    const JavaParamRef<jintArray>& data_types,
    jint time_period,
    const JavaParamRef<jobjectArray>& jexcluding_domains,
    const JavaParamRef<jintArray>& jexcluding_domain_reasons,
    const JavaParamRef<jobjectArray>& jignoring_domains,
    const JavaParamRef<jintArray>& jignoring_domain_reasons) {
  TRACE_EVENT0("browsing_data", "BrowsingDataBridge_ClearBrowsingData");

  Profile* profile = Profile::FromJavaObject(jprofile);
  BrowsingDataRemover* browsing_data_remover =
      profile->GetBrowsingDataRemover();

  std::vector<int> data_types_vector;
  base::android::JavaIntArrayToIntVector(env, data_types, &data_types_vector);

  uint64_t remove_mask = 0;
  for (const int data_type : data_types_vector) {
    switch (static_cast<browsing_data::BrowsingDataType>(data_type)) {
      case browsing_data::BrowsingDataType::HISTORY:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_HISTORY;
        break;
      case browsing_data::BrowsingDataType::CACHE:
        remove_mask |= BrowsingDataRemover::DATA_TYPE_CACHE;
        break;
      case browsing_data::BrowsingDataType::SITE_DATA:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
        break;
      case browsing_data::BrowsingDataType::PASSWORDS:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_PASSWORDS;
        remove_mask |=
            chrome_browsing_data_remover::DATA_TYPE_ACCOUNT_PASSWORDS;
        break;
      case browsing_data::BrowsingDataType::FORM_DATA:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_FORM_DATA;
        break;
      case browsing_data::BrowsingDataType::TABS:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_TABS;
        break;
      case browsing_data::BrowsingDataType::SITE_SETTINGS:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS;
        break;
      case browsing_data::BrowsingDataType::DOWNLOADS:
      case browsing_data::BrowsingDataType::HOSTED_APPS_DATA:
        // Only implemented on Desktop.
        NOTREACHED_IN_MIGRATION();
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
          content::BrowsingDataFilterBuilder::Mode::kPreserve));
  for (const std::string& domain : excluding_domains) {
    filter_builder->AddRegisterableDomain(domain);
  }

  if (!excluding_domains.empty() || !ignoring_domains.empty()) {
    site_engagement::ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
        profile, excluding_domains, excluding_domain_reasons, ignoring_domains,
        ignoring_domain_reasons);
  }

  base::OnceCallback<void(uint64_t)> callback = base::BindOnce(
      &OnBrowsingDataRemoverDone, ScopedJavaGlobalRef<jobject>(env, jcallback));

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
  Profile* profile = Profile::FromJavaObject(jprofile);
  browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
      SyncServiceFactory::GetForProfile(profile),
      WebHistoryServiceFactory::GetForProfile(profile), chrome::GetChannel(),
      base::BindOnce(&EnableDialogAboutOtherFormsOfBrowsingHistory,
                     ScopedJavaGlobalRef<jobject>(env, listener)));
}

static void JNI_BrowsingDataBridge_FetchImportantSites(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& java_callback) {
  TRACE_EVENT0("browsing_data", "BrowsingDataBridge_FetchImportantSites");
  Profile* profile = Profile::FromJavaObject(jprofile);
  std::vector<site_engagement::ImportantSitesUtil::ImportantDomainInfo>
      important_sites =
          site_engagement::ImportantSitesUtil::GetImportantRegisterableDomains(
              profile, site_engagement::ImportantSitesUtil::kMaxImportantSites);
  bool dialog_disabled =
      site_engagement::ImportantSitesUtil::IsDialogDisabled(profile);

  std::vector<std::string> important_domains;
  std::vector<int32_t> important_domain_reasons;
  std::vector<std::string> important_domain_examples;
  for (const site_engagement::ImportantSitesUtil::ImportantDomainInfo& info :
       important_sites) {
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
  return site_engagement::ImportantSitesUtil::kMaxImportantSites;
}

static void JNI_BrowsingDataBridge_MarkOriginAsImportantForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jorigin) {
  GURL origin(base::android::ConvertJavaStringToUTF8(jorigin));
  CHECK(origin.is_valid());
  site_engagement::ImportantSitesUtil::MarkOriginAsImportantForTesting(
      Profile::FromJavaObject(jprofile), origin);
}

static jboolean JNI_BrowsingDataBridge_GetBrowsingDataDeletionPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile,
    jint data_type,
    jint clear_browsing_data_tab) {
  DCHECK_GE(data_type, 0);
  DCHECK_LE(data_type,
            static_cast<int>(browsing_data::BrowsingDataType::MAX_VALUE));

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

  return GetPrefService(jprofile)->GetBoolean(pref);
}

static void JNI_BrowsingDataBridge_SetBrowsingDataDeletionPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile,
    jint data_type,
    jint clear_browsing_data_tab,
    jboolean value) {
  DCHECK_GE(data_type, 0);
  DCHECK_LE(data_type,
            static_cast<int>(browsing_data::BrowsingDataType::MAX_VALUE));

  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type),
          ToTabEnum(clear_browsing_data_tab), &pref)) {
    return;
  }

  GetPrefService(jprofile)->SetBoolean(pref, value);
}

static jint JNI_BrowsingDataBridge_GetBrowsingDataDeletionTimePeriod(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile,
    jint clear_browsing_data_tab) {
  return GetPrefService(jprofile)->GetInteger(
      browsing_data::GetTimePeriodPreferenceName(
          ToTabEnum(clear_browsing_data_tab)));
}

static void JNI_BrowsingDataBridge_SetBrowsingDataDeletionTimePeriod(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile,
    jint clear_browsing_data_tab,
    jint time_period) {
  DCHECK_GE(time_period, 0);
  DCHECK_LE(time_period,
            static_cast<int>(browsing_data::TimePeriod::TIME_PERIOD_LAST));
  const char* pref_name = browsing_data::GetTimePeriodPreferenceName(
      ToTabEnum(clear_browsing_data_tab));
  PrefService* prefs = GetPrefService(jprofile);
  int previous_value = prefs->GetInteger(pref_name);
  if (time_period != previous_value) {
    browsing_data::RecordTimePeriodChange(
        static_cast<browsing_data::TimePeriod>(time_period));
    prefs->SetInteger(pref_name, time_period);
  }
}

static jint JNI_BrowsingDataBridge_GetLastClearBrowsingDataTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile) {
  return GetPrefService(jprofile)->GetInteger(
      browsing_data::prefs::kLastClearBrowsingDataTab);
}

static void JNI_BrowsingDataBridge_SetLastClearBrowsingDataTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile,
    jint tab_index) {
  DCHECK_GE(tab_index, 0);
  DCHECK_LT(tab_index, 2);
  GetPrefService(jprofile)->SetInteger(
      browsing_data::prefs::kLastClearBrowsingDataTab, tab_index);
}

static void JNI_BrowsingDataBridge_BuildBrowsingDataModelFromDisk(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& java_callback) {
  Profile* profile = Profile::FromJavaObject(jprofile);
  BrowsingDataModel::BuildFromDisk(
      profile, ChromeBrowsingDataModelDelegate::CreateForProfile(profile),
      base::BindOnce(&OnBrowsingDataModelBuilt, env,
                     ScopedJavaGlobalRef<jobject>(java_callback)));
}
