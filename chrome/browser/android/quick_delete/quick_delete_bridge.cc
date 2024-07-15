// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/quick_delete/quick_delete_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/quick_delete/jni_headers/QuickDeleteBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

namespace {

struct QuickDeleteDomainResult {
  std::string last_visited_domain;
  size_t domain_count;
};

QuickDeleteDomainResult GetLastVisitedDomainAndUniqueDomainCountFromResult(
    const history::DomainsVisitedResult& result) {
  if (result.all_visited_domains.empty()) {
    return {"", 0};
  }

  return {result.all_visited_domains.front(),
          result.all_visited_domains.size()};
}
}  // namespace

QuickDeleteBridge::QuickDeleteBridge(Profile* profile) {
  profile_ = profile;

  history_service_ = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

QuickDeleteBridge::~QuickDeleteBridge() = default;

void QuickDeleteBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void QuickDeleteBridge::GetLastVisitedDomainAndUniqueDomainCount(
    JNIEnv* env,
    const jint time_period,
    const JavaParamRef<jobject>& j_callback) {
  browsing_data::TimePeriod period =
      static_cast<browsing_data::TimePeriod>(time_period);

  base::Time begin_time = CalculateBeginDeleteTime(period);
  base::Time end_time = CalculateEndDeleteTime(period);

  history_service_->GetUniqueDomainsVisited(
      begin_time, end_time,
      base::BindOnce(&QuickDeleteBridge::
                         OnGetLastVisitedDomainAndUniqueDomainCountComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_callback)),
      &task_tracker_);
}

void QuickDeleteBridge::ShowSurvey(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents_android) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents_android);
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);

  if (hats_service) {
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerQuickDelete, web_contents,
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
            messages::MessageIdentifier::PROMPT_HATS_QUICK_DELETE));
  }
}

// TODO(crbug.com/40255099) use rvalue reference to pass the result and define
// copy ctor and copy assignment in history::DomainsVisitedResult.
void QuickDeleteBridge::OnGetLastVisitedDomainAndUniqueDomainCountComplete(
    const JavaRef<jobject>& j_callback,
    history::DomainsVisitedResult result) {
  JNIEnv* env = AttachCurrentThread();
  QuickDeleteDomainResult quickDeleteResult =
      GetLastVisitedDomainAndUniqueDomainCountFromResult(std::move(result));

  Java_QuickDeleteBridge_onLastVisitedDomainAndUniqueDomainCountReady(
      env, j_callback,
      ConvertUTF8ToJavaString(env, quickDeleteResult.last_visited_domain),
      quickDeleteResult.domain_count);
}

static jlong JNI_QuickDeleteBridge_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        Profile* profile) {
  QuickDeleteBridge* bridge = new QuickDeleteBridge(profile);
  return reinterpret_cast<intptr_t>(bridge);
}
