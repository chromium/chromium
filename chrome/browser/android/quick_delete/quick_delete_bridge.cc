// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/quick_delete/quick_delete_bridge.h"

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/quick_delete/jni_headers/QuickDeleteBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

namespace {

struct QuickDeleteDomainResult {
  std::u16string last_visited_domain;
  int domain_count;
};

QuickDeleteDomainResult GetLastVisitedDomainAndUniqueDomainCountFromResult(
    const browsing_data::HistoryCounter::HistoryResult* result) {
  browsing_data::BrowsingDataCounter::ResultInt unique_domains_count =
      result->unique_domains_result();
  std::u16string last_visited_domain =
      base::UTF8ToUTF16(result->last_visited_domain());

  // Subtract one from the unique_domains_count since one of the domains will be
  // shown as the last_visited_domain.
  if (unique_domains_count > 0) {
    CHECK(!last_visited_domain.empty());
    unique_domains_count--;
  }

  return {last_visited_domain, static_cast<int>(unique_domains_count)};
}
}  // namespace

QuickDeleteBridge::QuickDeleteBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    Profile* profile)
    : jobject_(obj) {
  profile_ = profile;

  history_counter_ = std::make_unique<browsing_data::HistoryCounter>(
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      browsing_data::HistoryCounter::GetUpdatedWebHistoryServiceCallback(),
      SyncServiceFactory::GetForProfile(profile_));

  base::Time begin_time =
      CalculateBeginDeleteTime(browsing_data::TimePeriod::LAST_15_MINUTES);

  history_counter_->InitWithoutPeriodPref(
      profile_->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      begin_time,
      base::BindRepeating(&QuickDeleteBridge::OnHistoryCounterResult,
                          weak_ptr_factory_.GetWeakPtr()));
}

QuickDeleteBridge::~QuickDeleteBridge() = default;

void QuickDeleteBridge::Destroy(JNIEnv* env) {
  delete this;
}

void QuickDeleteBridge::RestartCounterForTimePeriod(JNIEnv* env,
                                                    const jint time_period) {
  browsing_data::TimePeriod period =
      static_cast<browsing_data::TimePeriod>(time_period);
  base::Time begin_time = CalculateBeginDeleteTime(period);

  history_counter_->SetBeginTime(begin_time);
}

void QuickDeleteBridge::OnHistoryCounterResult(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  JNIEnv* env = AttachCurrentThread();
  if (!result->Finished()) {
    return;
  }

  QuickDeleteDomainResult quickDeleteResult =
      GetLastVisitedDomainAndUniqueDomainCountFromResult(
          static_cast<const browsing_data::HistoryCounter::HistoryResult*>(
              result.get()));

  Java_QuickDeleteBridge_onLastVisitedDomainAndUniqueDomainCountReady(
      env, jobject_,
      ConvertUTF16ToJavaString(env, quickDeleteResult.last_visited_domain),
      quickDeleteResult.domain_count);
}

static jlong JNI_QuickDeleteBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    Profile* profile) {
  QuickDeleteBridge* bridge = new QuickDeleteBridge(env, obj, profile);
  return reinterpret_cast<intptr_t>(bridge);
}

DEFINE_JNI(QuickDeleteBridge)
