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
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/generated_resources.h"
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
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

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

PrefService* GetPrefService(Profile* profile) {
  return profile->GetOriginalProfile()->GetPrefs();
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
    Profile* profile,
    const JavaParamRef<jobject>& jcallback,
    std::vector<int>& data_types_vector,
    jint time_period,
    std::vector<std::string>& excluding_domains,
    std::vector<int32_t>& excluding_domain_reasons,
    std::vector<std::string>& ignoring_domains,
    std::vector<int32_t>& ignoring_domain_reasons) {
  TRACE_EVENT0("browsing_data", "BrowsingDataBridge_ClearBrowsingData");

  BrowsingDataRemover* browsing_data_remover =
      profile->GetBrowsingDataRemover();

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
        NOTREACHED();
    }
  }
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
    Profile* profile,
    const JavaParamRef<jobject>& listener) {
  TRACE_EVENT0(
      "browsing_data",
      "BrowsingDataBridge_RequestInfoAboutOtherFormsOfBrowsingHistory");
  // The one-time notice in the dialog.
  browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
      SyncServiceFactory::GetForProfile(profile),
      WebHistoryServiceFactory::GetForProfile(profile), chrome::GetChannel(),
      base::BindOnce(&EnableDialogAboutOtherFormsOfBrowsingHistory,
                     ScopedJavaGlobalRef<jobject>(env, listener)));
}

static void JNI_BrowsingDataBridge_FetchImportantSites(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& java_callback) {
  TRACE_EVENT0("browsing_data", "BrowsingDataBridge_FetchImportantSites");
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

  Java_ImportantSitesCallback_onImportantRegisterableDomainsReady(
      env, java_callback, important_domains, important_domain_examples,
      important_domain_reasons, dialog_disabled);
}

// This value should not change during a sessions, as it's used for UMA metrics.
static jint JNI_BrowsingDataBridge_GetMaxImportantSites(JNIEnv* env) {
  return site_engagement::ImportantSitesUtil::kMaxImportantSites;
}

static void JNI_BrowsingDataBridge_MarkOriginAsImportantForTesting(
    JNIEnv* env,
    Profile* profile,
    std::string& jorigin) {
  GURL origin(jorigin);
  CHECK(origin.is_valid());
  site_engagement::ImportantSitesUtil::MarkOriginAsImportantForTesting(profile,
                                                                       origin);
}

static jboolean JNI_BrowsingDataBridge_GetBrowsingDataDeletionPreference(
    JNIEnv* env,
    Profile* profile,
    jint data_type) {
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
          browsing_data::ClearBrowsingDataTab::ADVANCED, &pref)) {
    return false;
  }

  return GetPrefService(profile)->GetBoolean(pref);
}

static void JNI_BrowsingDataBridge_SetBrowsingDataDeletionPreference(
    JNIEnv* env,
    Profile* profile,
    jint data_type,
    jboolean value) {
  DCHECK_GE(data_type, 0);
  DCHECK_LE(data_type,
            static_cast<int>(browsing_data::BrowsingDataType::MAX_VALUE));

  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type),
          browsing_data::ClearBrowsingDataTab::ADVANCED, &pref)) {
    return;
  }

  GetPrefService(profile)->SetBoolean(pref, value);
}

static jint JNI_BrowsingDataBridge_GetBrowsingDataDeletionTimePeriod(
    JNIEnv* env,
    Profile* profile) {
  return GetPrefService(profile)->GetInteger(
      browsing_data::GetTimePeriodPreferenceName(
          browsing_data::ClearBrowsingDataTab::ADVANCED));
}

static void JNI_BrowsingDataBridge_SetBrowsingDataDeletionTimePeriod(
    JNIEnv* env,
    Profile* profile,
    jint time_period) {
  DCHECK_GE(time_period, 0);
  DCHECK_LE(time_period,
            static_cast<int>(browsing_data::TimePeriod::TIME_PERIOD_LAST));
  const char* pref_name = browsing_data::GetTimePeriodPreferenceName(
      browsing_data::ClearBrowsingDataTab::ADVANCED);
  PrefService* prefs = GetPrefService(profile);
  int previous_value = prefs->GetInteger(pref_name);
  if (time_period != previous_value) {
    browsing_data::RecordTimePeriodChange(
        static_cast<browsing_data::TimePeriod>(time_period));
    prefs->SetInteger(pref_name, time_period);
  }
}

static void JNI_BrowsingDataBridge_BuildBrowsingDataModelFromDisk(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& java_callback) {
  BrowsingDataModel::BuildFromDisk(
      profile, ChromeBrowsingDataModelDelegate::CreateForProfile(profile),
      base::BindOnce(&OnBrowsingDataModelBuilt, env,
                     ScopedJavaGlobalRef<jobject>(java_callback)));
}

static void JNI_BrowsingDataBridge_TriggerHatsSurvey(
    JNIEnv* env,
    Profile* profile,
    content::WebContents* web_contents,
    jboolean quick_delete) {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);

  std::string trigger = quick_delete ? kHatsSurveyTriggerQuickDelete
                                     : kHatsSurveyTriggerClearBrowsingData;
  messages::MessageIdentifier message_id =
      quick_delete
          ? messages::MessageIdentifier::PROMPT_HATS_QUICK_DELETE
          : messages::MessageIdentifier::PROMPT_HATS_CLEAR_BROWSING_DATA;

  if (hats_service) {
    hats_service->LaunchDelayedSurveyForWebContents(
        trigger, web_contents,
        /*timeout_ms=*/5000,
        /*product_specific_bits_data=*/{},
        /*product_specific_string_data=*/{},
        HatsService::NavigationBehaviour::ALLOW_ANY,
        /*success_callback=*/base::DoNothing(),
        /*failure_callback=*/base::DoNothing(),
        /*supplied_trigger_id=*/std::nullopt,
        HatsService::SurveyOptions(
            l10n_util::GetStringUTF16(
                IDS_QUICK_DELETE_PROMPT_SURVEY_CUSTOM_INVITATION),
            message_id));
  }
}
