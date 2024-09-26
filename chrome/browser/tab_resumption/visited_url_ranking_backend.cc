// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_resumption/visited_url_ranking_backend.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/android/jni_string.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab_resumption/jni_headers/VisitedUrlRankingBackend_jni.h"

using base::android::ScopedJavaLocalRef;
using tab_resumption::jni::SuggestionEntryType;
using visited_url_ranking::DecorationType;
using visited_url_ranking::GetStringForDecoration;
using visited_url_ranking::GetStringForRecencyDecorationWithTime;

namespace {

using Source = visited_url_ranking::URLVisit::Source;
using FetchSources =
    base::EnumSet<Source, Source::kNotApplicable, Source::kForeign>;
using tab_resumption::jni::Java_VisitedUrlRankingBackend_addSuggestionEntry;
using tab_resumption::jni::Java_VisitedUrlRankingBackend_onSuggestions;
using tab_resumption::jni::VisitedUrlRankingBackend;

using visited_url_ranking::Config;
using visited_url_ranking::Fetcher;
using visited_url_ranking::FetchOptions;
using visited_url_ranking::ResultStatus;
using visited_url_ranking::ScoredURLUserAction;
using visited_url_ranking::URLVisitAggregate;
using visited_url_ranking::URLVisitAggregatesTransformType;
using visited_url_ranking::URLVisitsMetadata;
using visited_url_ranking::VisitedURLRankingService;

// Must match Java Tab.INVALID_TAB_ID.
static constexpr int kInvalidTabId = -1;

// FetchOptions::CreateDefaultFetchOptionsForTabResumption() specifies data
// sources that are currently unavailable. This function returns a simplified
// FetchOptions instance.
FetchOptions CreateFetchOptionsForTabResumption(base::Time current_time,
                                                bool fetch_history) {
  FetchOptions::URLTypeSet expected_types = {
      FetchOptions::URLType::kActiveRemoteTab};
  if (fetch_history) {
    expected_types.Put(FetchOptions::URLType::kActiveLocalTab);
    expected_types.Put(FetchOptions::URLType::kLocalVisit);
    expected_types.Put(FetchOptions::URLType::kRemoteVisit);
    expected_types.Put(FetchOptions::URLType::kCCTVisit);
  }
  return FetchOptions::CreateFetchOptionsForTabResumption(expected_types);
}

// Class to manage tab resumption fetch and rank flow, containing required
// parameters and states
class FetchAndRankFlow : public base::RefCounted<FetchAndRankFlow> {
 public:
  friend base::RefCounted<FetchAndRankFlow>;

  FetchAndRankFlow(Profile* profile,
                   JNIEnv* env,
                   jni_zero::ScopedJavaGlobalRef<jobject> jobj,
                   base::Time current_time,
                   bool fetch_history,
                   jni_zero::ScopedJavaGlobalRef<jobject> j_suggestions,
                   jni_zero::ScopedJavaGlobalRef<jobject> j_callback,
                   DecorationType decoration_type_override)
      : ranking_service_(
            visited_url_ranking::VisitedURLRankingServiceFactory::GetInstance()
                ->GetForProfile(profile)),
        env_(env),
        jobj_(jobj),
        j_suggestions_(j_suggestions),
        j_callback_(j_callback),
        fetch_options_(
            CreateFetchOptionsForTabResumption(current_time, fetch_history)),
        config_({.key = visited_url_ranking::kTabResumptionRankerKey}),
        decoration_type_override_(decoration_type_override) {}

  void RunFlow() {
    ranking_service_->FetchURLVisitAggregates(
        fetch_options_,
        base::BindOnce(&FetchAndRankFlow::OnFetched, base::RetainedRef(this)));
  }

 private:
  ~FetchAndRankFlow() = default;

  // Continuing after RunFlow()'s call to FetchURLVisitAggregates().
  void OnFetched(ResultStatus status,
                 URLVisitsMetadata url_visits_metadata,
                 std::vector<URLVisitAggregate> aggregates) {
    if (status != ResultStatus::kSuccess) {
      Java_VisitedUrlRankingBackend_onSuggestions(env_, j_suggestions_,
                                                  j_callback_);
      return;
    }

    ranking_service_->RankURLVisitAggregates(
        config_, std::move(aggregates),
        base::BindOnce(&FetchAndRankFlow::OnRanked, base::RetainedRef(this)));
  }

  // Continuing after OnFetched()'s call to RankVisitAggregates().
  void OnRanked(ResultStatus status,
                std::vector<URLVisitAggregate> aggregates) {
    if (status != ResultStatus::kSuccess) {
      Java_VisitedUrlRankingBackend_onSuggestions(env_, j_suggestions_,
                                                  j_callback_);
      return;
    }

    ranking_service_->DecorateURLVisitAggregates(
        {}, std::move(aggregates),
        base::BindOnce(&FetchAndRankFlow::PassResults,
                       base::RetainedRef(this)));
  }

  // Translates results to Java objects and passes results to |j_callback_|.
  void PassResults(visited_url_ranking::ResultStatus status,
                   std::vector<URLVisitAggregate> aggregates) {
    std::u16string decoration_override;
    if (decoration_type_override_ != DecorationType::kUnknown &&
        decoration_type_override_ != DecorationType::kVisitedXAgo) {
      decoration_override = GetStringForDecoration(decoration_type_override_);
    }

    for (const URLVisitAggregate& aggregate : aggregates) {
      // TODO(crbug.com/337858147): Choose representative member. For now, just
      // take the first one.
      if (aggregate.fetcher_data_map.empty()) {
        continue;
      }

      if (decoration_type_override_ == DecorationType::kVisitedXAgo) {
        decoration_override =
            GetStringForRecencyDecorationWithTime(aggregate.GetLastVisitTime());
      }

      std::optional<ScopedJavaLocalRef<jstring>> decoration;
      if (!decoration_override.empty()) {
        decoration =
            base::android::ConvertUTF16ToJavaString(env_, decoration_override);
      } else if (!aggregate.decorations.empty()) {
        decoration = base::android::ConvertUTF16ToJavaString(
            env_, GetMostRelevantDecoration(aggregate).GetDisplayString());
      }

      const auto& fetcher_entry = *aggregate.fetcher_data_map.begin();
      std::visit(
          visited_url_ranking::URLVisitVariantHelper{
              [&](const URLVisitAggregate::TabData& tab_data) {
                bool is_local_tab =
                    (tab_data.last_active_tab.session_tag == std::nullopt);
                Java_VisitedUrlRankingBackend_addSuggestionEntry(
                    env_, jobj_,
                    JniIntWrapper(static_cast<int>(
                        is_local_tab ? SuggestionEntryType::kLocalTab
                                     : SuggestionEntryType::kForeignTab)),
                    base::android::ConvertUTF8ToJavaString(
                        env_,
                        tab_data.last_active_tab.session_name.value_or("")),
                    url::GURLAndroid::FromNativeGURL(
                        env_, tab_data.last_active_tab.visit.url),
                    base::android::ConvertUTF16ToJavaString(
                        env_, tab_data.last_active_tab.visit.title),
                    tab_data.last_active.InMillisecondsSinceUnixEpoch(),
                    is_local_tab ? tab_data.last_active_tab.id : kInvalidTabId,
                    base::android::ConvertUTF8ToJavaString(env_,
                                                           aggregate.url_key),
                    aggregate.request_id.is_null()
                        ? -1LL
                        : aggregate.request_id.GetUnsafeValue(),
                    nullptr, decoration.value_or(nullptr), !is_local_tab,
                    j_suggestions_);
              },
              [&](const URLVisitAggregate::HistoryData& history_data) {
                bool need_match_local_tab =
                    history_data.last_visited.context_annotations.on_visit
                        .browser_type ==
                    history::VisitContextAnnotations::BrowserType::kTabbed;
                Java_VisitedUrlRankingBackend_addSuggestionEntry(
                    env_, jobj_,
                    JniIntWrapper(
                        static_cast<int>(SuggestionEntryType::kHistory)),
                    base::android::ConvertUTF8ToJavaString(
                        env_, history_data.visit.client_name.value_or("")),
                    url::GURLAndroid::FromNativeGURL(
                        env_, history_data.last_visited.url_row.url()),
                    base::android::ConvertUTF16ToJavaString(
                        env_, history_data.last_visited.url_row.title()),
                    history_data.last_visited.visit_row.visit_time
                        .InMillisecondsSinceUnixEpoch(),
                    kInvalidTabId,
                    base::android::ConvertUTF8ToJavaString(env_,
                                                           aggregate.url_key),
                    aggregate.request_id.is_null()
                        ? -1LL
                        : aggregate.request_id.GetUnsafeValue(),
                    history_data.last_app_id
                        ? base::android::ConvertUTF8ToJavaString(
                              env_, *history_data.last_app_id)
                        : nullptr,
                    decoration.value_or(nullptr), need_match_local_tab,
                    j_suggestions_);
              }},
          fetcher_entry.second);
    }

    Java_VisitedUrlRankingBackend_onSuggestions(env_, j_suggestions_,
                                                j_callback_);
  }

 private:
  raw_ptr<visited_url_ranking::VisitedURLRankingService> ranking_service_;
  raw_ptr<JNIEnv> env_;
  jni_zero::ScopedJavaGlobalRef<jobject> jobj_;
  jni_zero::ScopedJavaGlobalRef<jobject> j_suggestions_;
  jni_zero::ScopedJavaGlobalRef<jobject> j_callback_;
  const FetchOptions fetch_options_;
  const Config config_;
  const DecorationType decoration_type_override_;
};

}  // namespace

namespace tab_resumption::jni {

static jlong JNI_VisitedUrlRankingBackend_Init(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& jobj,
    Profile* profile) {
  return reinterpret_cast<intptr_t>(
      new VisitedUrlRankingBackend(jobj, profile));
}

VisitedUrlRankingBackend::VisitedUrlRankingBackend(
    const jni_zero::JavaRef<jobject>& jobj,
    Profile* profile)
    : jobj_(jni_zero::ScopedJavaGlobalRef<jobject>(jobj)), profile_(profile) {
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);

  decoration_type_override_ = static_cast<visited_url_ranking::DecorationType>(
      base::GetFieldTrialParamByFeatureAsInt(
          chrome::android::kTabResumptionModuleAndroid, "override_decoration",
          0));

  // SessionSyncService can be null in tests.
  if (session_sync_service) {
    // base::Unretained() is safe below because the subscription itself is a
    // class member field and handles destruction well.
    foreign_session_updated_subscription_ =
        session_sync_service->SubscribeToForeignSessionsChanged(
            base::BindRepeating(&VisitedUrlRankingBackend::OnRefresh,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

VisitedUrlRankingBackend::~VisitedUrlRankingBackend() = default;

void VisitedUrlRankingBackend::Destroy(JNIEnv* env) {
  jobj_ = nullptr;
  delete this;
}

void VisitedUrlRankingBackend::TriggerUpdate(JNIEnv* env) {
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  service->TriggerRefresh({syncer::SESSIONS});
}

void VisitedUrlRankingBackend::GetRankedSuggestions(
    JNIEnv* env,
    jlong current_time_ms,
    jboolean fetch_history,
    const jni_zero::JavaParamRef<jobject>& suggestions,
    const jni_zero::JavaParamRef<jobject>& callback) {
  jni_zero::ScopedJavaGlobalRef<jobject> j_suggestions(env, suggestions);
  jni_zero::ScopedJavaGlobalRef<jobject> j_callback(env, callback);

  auto current_time =
      base::Time::FromMillisecondsSinceUnixEpoch(current_time_ms);
  scoped_refptr<FetchAndRankFlow> flow = base::MakeRefCounted<FetchAndRankFlow>(
      profile_, env, jobj_, current_time, fetch_history, j_suggestions,
      j_callback, decoration_type_override_);

  flow->RunFlow();
}

void VisitedUrlRankingBackend::RecordAction(JNIEnv* env,
                                            jint scored_url_user_action,
                                            jstring visit_id,
                                            jlong visit_request_id) {
  visited_url_ranking::VisitedURLRankingService* ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetInstance()
          ->GetForProfile(profile_);
  if (!ranking_service) {
    return;
  }
  ranking_service->RecordAction(
      static_cast<ScoredURLUserAction>(scored_url_user_action),
      base::android::ConvertJavaStringToUTF8(env, visit_id),
      segmentation_platform::TrainingRequestId::FromUnsafeValue(
          visit_request_id));
}

void VisitedUrlRankingBackend::OnRefresh() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_VisitedUrlRankingBackend_onRefresh(env, jobj_);
}

}  // namespace tab_resumption::jni
