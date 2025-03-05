// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_provider.h"

#include <algorithm>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/auxiliary_search/proto/auxiliary_search_group.pb.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "url/android/gurl_android.h"
#include "url/url_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/auxiliary_search/jni_headers/AuxiliarySearchBridge_jni.h"

using base::android::ToJavaByteArray;
using visited_url_ranking::Config;
using visited_url_ranking::Fetcher;
using visited_url_ranking::FetchOptions;
using visited_url_ranking::ResultStatus;
using visited_url_ranking::URLVisitAggregate;
using visited_url_ranking::URLVisitAggregatesTransformType;
using visited_url_ranking::URLVisitsMetadata;
using visited_url_ranking::URLVisitVariantHelper;
using visited_url_ranking::VisitedURLRankingService;
using visited_url_ranking::VisitedURLRankingServiceFactory;

namespace {
// Must match Java Tab.INVALID_TAB_ID.
static constexpr int kInvalidTabId = -1;

// 1 day in hours.
const int kHistoryAgeThresholdHoursDefaultValue = 24;
// 7 days in hours.
const int kTabAgeThresholdHoursDefaultValue = 168;

using BackToJavaCallback = base::OnceCallback<void(
    std::unique_ptr<std::vector<base::WeakPtr<TabAndroid>>>)>;

class AuxiliarySearchProviderFactory : public ProfileKeyedServiceFactory {
 public:
  static AuxiliarySearchProvider* GetForProfile(Profile* profile) {
    return static_cast<AuxiliarySearchProvider*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static AuxiliarySearchProviderFactory* GetInstance() {
    return base::Singleton<AuxiliarySearchProviderFactory>::get();
  }

  AuxiliarySearchProviderFactory()
      : ProfileKeyedServiceFactory(
            "AuxiliarySearchProvider",
            ProfileSelections::Builder()
                .WithRegular(ProfileSelection::kRedirectedToOriginal)
                .WithGuest(ProfileSelection::kNone)
                .Build()) {
    if (base::FeatureList::IsEnabled(
            chrome::android::kAndroidAppIntegrationMultiDataSource)) {
      DependsOn(VisitedURLRankingServiceFactory::GetInstance());
    }
  }

 private:
  // ProfileKeyedServiceFactory overrides
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    DCHECK(!profile->IsOffTheRecord());

    if (base::FeatureList::IsEnabled(
            chrome::android::kAndroidAppIntegrationMultiDataSource)) {
      return std::make_unique<AuxiliarySearchProvider>(
          VisitedURLRankingServiceFactory::GetForProfile(profile));
    }
    return std::make_unique<AuxiliarySearchProvider>(nullptr);
  }
};

void CallJavaCallbackWithTabList(
    JNIEnv* env,
    const base::android::ScopedJavaGlobalRef<jobject>& j_callback_obj,
    std::vector<base::WeakPtr<TabAndroid>> non_sensitive_tabs) {
  DCHECK_LE(non_sensitive_tabs.size(),
            chrome::android::kAuxiliarySearchMaxTabsCountParam.Get());
  std::vector<base::android::ScopedJavaLocalRef<jobject>> j_tabs_list;
  std::ranges::transform(non_sensitive_tabs, std::back_inserter(j_tabs_list),
                         [](const auto& tab) { return tab->GetJavaObject(); });
  base::android::RunObjectCallbackAndroid(
      j_callback_obj, base::android::ToJavaArrayOfObjects(env, j_tabs_list));
}

bool IsSchemeAllowed(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

base::WeakPtr<TabAndroid> FilterNonSensitiveSearchableTab(
    base::WeakPtr<TabAndroid> tab,
    PersistedTabDataAndroid* persisted_tab_data) {
  if (!tab) {
    return nullptr;
  }

  // PersistedTabAndroid::From() can yield nullptr, but the only time that
  // should happen in this code is if `tab` is gone; otherwise, it implies code
  // is unexpectedly clearing `SensitivityPersistedTabDataAndroid`.
  SensitivityPersistedTabDataAndroid* sensitivity_persisted_tab_data_android =
      static_cast<SensitivityPersistedTabDataAndroid*>(persisted_tab_data);

  if (sensitivity_persisted_tab_data_android->is_sensitive()) {
    return nullptr;
  }

  return tab;
}

// Get the default age limit for the `url_type`.
base::TimeDelta GetDefaultAgeLimit(URLVisitAggregate::URLType url_type) {
  switch (url_type) {
    case URLVisitAggregate::URLType::kActiveLocalTab:
      return base::Hours(kTabAgeThresholdHoursDefaultValue);
    case URLVisitAggregate::URLType::kCCTVisit:
      return base::Hours(kHistoryAgeThresholdHoursDefaultValue);
    default:
      return base::TimeDelta();
  }
}

FetchOptions CreateFetchOptionsForTabDonation(
    const URLVisitAggregate::URLTypeSet& result_sources) {
  std::vector<URLVisitAggregatesTransformType> transforms{
      URLVisitAggregatesTransformType::kRecencyFilter,
      URLVisitAggregatesTransformType::kDefaultAppUrlFilter,
      URLVisitAggregatesTransformType::kHistoryBrowserTypeFilter,
  };

  if (base::FeatureList::IsEnabled(
          visited_url_ranking::features::
              kVisitedURLRankingHistoryVisibilityScoreFilter)) {
    transforms.push_back(
        URLVisitAggregatesTransformType::kHistoryVisibilityScoreFilter);
  }

  std::map<Fetcher, visited_url_ranking::FetchOptions::FetchSources>
      fetcher_sources;
  // Always useful for signals.
  fetcher_sources.emplace(Fetcher::kHistory,
                          visited_url_ranking::FetchOptions::kOriginSources);

  fetcher_sources.emplace(Fetcher::kTabModel,
                          visited_url_ranking::FetchOptions::FetchSources(
                              {visited_url_ranking::URLVisit::Source::kLocal}));

  // Sets the query duration to match the age limit for the local Tabs. It
  // allows getting the sensitivity scores of all qualified local Tabs.
  int query_duration = base::GetFieldTrialParamByFeatureAsInt(
      visited_url_ranking::features::kVisitedURLRankingService,
      visited_url_ranking::features::
          kVisitedURLRankingFetchDurationInHoursParam,
      kTabAgeThresholdHoursDefaultValue);
  std::map<URLVisitAggregate::URLType,
           visited_url_ranking::FetchOptions::ResultOption>
      result_map;
  for (URLVisitAggregate::URLType type : result_sources) {
    result_map[type] = visited_url_ranking::FetchOptions::ResultOption{
        .age_limit = GetDefaultAgeLimit(type)};
  }
  return FetchOptions(std::move(result_map), std::move(fetcher_sources),
                      base::Time::Now() - base::Hours(query_duration),
                      std::move(transforms));
}

FetchOptions CreateFetchOptions() {
  URLVisitAggregate::URLTypeSet expected_types = {
      URLVisitAggregate::URLType::kActiveLocalTab,
      URLVisitAggregate::URLType::kCCTVisit};
  return CreateFetchOptionsForTabDonation(expected_types);
}

// Class to manage history data fetch and rank flow, containing required
// parameters and states.
class FetchAndRankHelper : public base::RefCounted<FetchAndRankHelper> {
 public:
  friend base::RefCounted<FetchAndRankHelper>;

  FetchAndRankHelper(VisitedURLRankingService* ranking_service,
                     JNIEnv* env,
                     base::android::ScopedJavaGlobalRef<jobject> j_ref,
                     base::android::ScopedJavaGlobalRef<jobject> j_entries,
                     base::android::ScopedJavaGlobalRef<jobject> j_callback)
      : ranking_service_(ranking_service),
        env_(env),
        j_ref_(j_ref),
        j_entries_(j_entries),
        j_callback_(j_callback),
        fetch_options_(CreateFetchOptions()),
        config_({.key = visited_url_ranking::kTabResumptionRankerKey}) {}

  void StartFetching() {
    ranking_service_->FetchURLVisitAggregates(
        fetch_options_, base::BindOnce(&FetchAndRankHelper::OnFetched,
                                       base::RetainedRef(this)));
  }

 private:
  ~FetchAndRankHelper() = default;

  // Continuing after StartFetching()'s call to FetchURLVisitAggregates().
  void OnFetched(ResultStatus status,
                 URLVisitsMetadata url_visits_metadata,
                 std::vector<URLVisitAggregate> aggregates) {
    if (status != ResultStatus::kSuccess) {
      Java_AuxiliarySearchBridge_onDataReady(env_, j_ref_, j_entries_,
                                             j_callback_);
      return;
    }

    ranking_service_->RankURLVisitAggregates(
        config_, std::move(aggregates),
        base::BindOnce(&FetchAndRankHelper::OnRanked, base::RetainedRef(this),
                       std::move(url_visits_metadata)));
  }

  // Continuing after OnFetched()'s call to RankVisitAggregates().
  void OnRanked(URLVisitsMetadata url_visits_metadata,
                ResultStatus status,
                std::vector<URLVisitAggregate> aggregates) {
    if (status != ResultStatus::kSuccess) {
      Java_AuxiliarySearchBridge_onDataReady(env_, j_ref_, j_entries_,
                                             j_callback_);
      return;
    }

    for (const URLVisitAggregate& aggregate : aggregates) {
      if (aggregate.fetcher_data_map.empty()) {
        continue;
      }
      // TODO(crbug.com/337858147): Choose representative member. For now, just
      // take the first one.
      const auto& fetcher_entry = *aggregate.fetcher_data_map.begin();
      std::visit(
          URLVisitVariantHelper{
              [&](const URLVisitAggregate::TabData& tab_data) {
                bool is_local_tab =
                    (tab_data.last_active_tab.id != kInvalidTabId);
                if (!is_local_tab) {
                  return;
                }

                Java_AuxiliarySearchBridge_addDataEntry(
                    env_, j_ref_,
                    JniIntWrapper(
                        static_cast<int>(AuxiliarySearchEntryType::kTab)),
                    url::GURLAndroid::FromNativeGURL(
                        env_, tab_data.last_active_tab.visit.url),
                    base::android::ConvertUTF16ToJavaString(
                        env_, tab_data.last_active_tab.visit.title),
                    tab_data.last_active.InMillisecondsSinceUnixEpoch(),
                    tab_data.last_active_tab.id, /* appId= */ nullptr,
                    kInvalidTabId, j_entries_);
              },
              [&](const URLVisitAggregate::HistoryData& history_data) {
                bool is_custom_tab =
                    history_data.last_visited.context_annotations.on_visit
                        .browser_type ==
                    history::VisitContextAnnotations::BrowserType::kCustomTab;
                if (!is_custom_tab) {
                  return;
                }

                Java_AuxiliarySearchBridge_addDataEntry(
                    env_, j_ref_,
                    JniIntWrapper(
                        static_cast<int>(AuxiliarySearchEntryType::kCustomTab)),
                    url::GURLAndroid::FromNativeGURL(
                        env_, history_data.last_visited.url_row.url()),
                    base::android::ConvertUTF16ToJavaString(
                        env_, history_data.last_visited.url_row.title()),
                    history_data.last_visited.visit_row.visit_time
                        .InMillisecondsSinceUnixEpoch(),
                    kInvalidTabId,
                    history_data.last_app_id
                        ? base::android::ConvertUTF8ToJavaString(
                              env_, *history_data.last_app_id)
                        : nullptr,
                    base::Hash(aggregate.url_key), j_entries_);
              }},
          fetcher_entry.second);
    }

    Java_AuxiliarySearchBridge_onDataReady(env_, j_ref_, j_entries_,
                                           j_callback_);
  }

 private:
  raw_ptr<VisitedURLRankingService> ranking_service_;
  raw_ptr<JNIEnv> env_;
  base::android::ScopedJavaGlobalRef<jobject> j_ref_;
  base::android::ScopedJavaGlobalRef<jobject> j_entries_;
  base::android::ScopedJavaGlobalRef<jobject> j_callback_;
  const FetchOptions fetch_options_;
  const Config config_;
};

}  // namespace

AuxiliarySearchProvider::AuxiliarySearchProvider(
    VisitedURLRankingService* ranking_service)
    : ranking_service_(ranking_service) {}

AuxiliarySearchProvider::~AuxiliarySearchProvider() = default;

void AuxiliarySearchProvider::GetNonSensitiveTabs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& j_tabs_android,
    const base::android::JavaParamRef<jobject>& j_callback_obj) const {
  std::vector<raw_ptr<TabAndroid, VectorExperimental>> all_tabs =
      TabAndroid::GetAllNativeTabs(
          env, base::android::ScopedJavaLocalRef<jobjectArray>(j_tabs_android));

  GetNonSensitiveTabsInternal(
      std::move(all_tabs),
      base::BindOnce(
          &CallJavaCallbackWithTabList, env,
          base::android::ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void AuxiliarySearchProvider::GetNonSensitiveHistoryData(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_ref_obj,
    const base::android::JavaParamRef<jobject>& j_entries_obj,
    const base::android::JavaParamRef<jobject>& j_callback_obj) const {
  base::android::ScopedJavaGlobalRef<jobject> j_ref(j_ref_obj);
  base::android::ScopedJavaGlobalRef<jobject> j_entries(env, j_entries_obj);

  base::android::ScopedJavaGlobalRef<jobject> j_callback(env, j_callback_obj);

  CHECK(ranking_service_ != nullptr);
  scoped_refptr<FetchAndRankHelper> helper =
      base::MakeRefCounted<FetchAndRankHelper>(ranking_service_, env, j_ref,
                                               j_entries, j_callback);

  helper->StartFetching();
}

// static
void AuxiliarySearchProvider::FilterTabsByScheme(
    std::vector<raw_ptr<TabAndroid, VectorExperimental>>& tabs) {
  std::erase_if(
      tabs, [](const auto& tab) { return !IsSchemeAllowed(tab->GetURL()); });
}

void AuxiliarySearchProvider::GetNonSensitiveTabsInternal(
    std::vector<raw_ptr<TabAndroid, VectorExperimental>> all_tabs,
    NonSensitiveTabsCallback callback) const {
  FilterTabsByScheme(all_tabs);

  auto barrier_cb = base::BarrierCallback<base::WeakPtr<TabAndroid>>(
      all_tabs.size(),
      // Filter out any tabs that are no longer live and ensure the results
      // are capped if needed.
      //
      // In theory, this could be folded into CallJavaCallbackWithTabList
      // instead of using a trampoline callback, but some tests exercise this
      // helper function directly.
      base::BindOnce([](std::vector<base::WeakPtr<TabAndroid>> tabs) {
        std::erase_if(tabs, [](const auto& tab) { return !tab; });
        const size_t max_tabs =
            chrome::android::kAuxiliarySearchMaxTabsCountParam.Get();
        if (tabs.size() > max_tabs) {
          tabs.resize(max_tabs);
        }
        return tabs;
      }).Then(std::move(callback)));

  for (const auto& tab : all_tabs) {
    SensitivityPersistedTabDataAndroid::From(
        tab, base::BindOnce(&FilterNonSensitiveSearchableTab, tab->GetWeakPtr())
                 .Then(barrier_cb));
  }
}

// static
jlong JNI_AuxiliarySearchBridge_GetForProfile(JNIEnv* env, Profile* profile) {
  DCHECK(profile);

  return reinterpret_cast<intptr_t>(
      AuxiliarySearchProviderFactory::GetForProfile(profile));
}

// static
void AuxiliarySearchProvider::EnsureFactoryBuilt() {
  AuxiliarySearchProviderFactory::GetInstance();
}
