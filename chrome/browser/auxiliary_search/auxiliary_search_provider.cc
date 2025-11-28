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
#include "chrome/browser/auxiliary_search/fetch_and_rank_helper.h"
#include "chrome/browser/auxiliary_search/proto/auxiliary_search_group.pb.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "components/ntp_tiles/constants.h"
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
using visited_url_ranking::VisitedURLRankingService;
using visited_url_ranking::VisitedURLRankingServiceFactory;

namespace {
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

void OnDataReady(JNIEnv* env,
                 base::android::ScopedJavaGlobalRef<jobject> j_callback,
                 std::vector<jni_zero::ScopedJavaLocalRef<jobject>> entries,
                 const visited_url_ranking::URLVisitsMetadata& metadata) {
  Java_AuxiliarySearchBridge_onDataReady(env, entries, j_callback);
}

}  // namespace

AuxiliarySearchProvider::AuxiliarySearchProvider(
    VisitedURLRankingService* ranking_service)
    : ranking_service_(ranking_service) {}

AuxiliarySearchProvider::~AuxiliarySearchProvider() = default;

void AuxiliarySearchProvider::GetNonSensitiveTabs(
    JNIEnv* env,
    std::vector<TabAndroid*> tabs,
    const base::android::JavaParamRef<jobject>& j_callback_obj) const {
  GetNonSensitiveTabsInternal(
      std::move(tabs),
      base::BindOnce(
          &CallJavaCallbackWithTabList, env,
          base::android::ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

void AuxiliarySearchProvider::GetNonSensitiveHistoryData(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_callback_obj) const {
  CHECK(ranking_service_ != nullptr);
  scoped_refptr<FetchAndRankHelper> helper =
      base::MakeRefCounted<FetchAndRankHelper>(
          ranking_service_,
          base::BindOnce(
              &OnDataReady, env,
              base::android::ScopedJavaGlobalRef<jobject>(j_callback_obj)));

  helper->StartFetching();
}

void AuxiliarySearchProvider::GetCustomTabs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url,
    jlong j_begin_time,
    const base::android::JavaParamRef<jobject>& j_callback_obj) const {
  CHECK(ranking_service_ != nullptr);
  scoped_refptr<FetchAndRankHelper> helper =
      base::MakeRefCounted<FetchAndRankHelper>(
          ranking_service_,
          base::BindOnce(
              &OnDataReady, env,
              base::android::ScopedJavaGlobalRef<jobject>(j_callback_obj)),
          url::GURLAndroid::ToNativeGURL(env, j_url),
          base::Time::FromMillisecondsSinceUnixEpoch(j_begin_time));

  helper->StartFetching();
}

// static
void AuxiliarySearchProvider::FilterTabsByScheme(
    std::vector<TabAndroid*>& tabs) {
  std::erase_if(
      tabs, [](const auto& tab) { return !IsSchemeAllowed(tab->GetURL()); });
}

void AuxiliarySearchProvider::GetNonSensitiveTabsInternal(
    std::vector<TabAndroid*> all_tabs,
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
        tab, base::BindOnce(&FilterNonSensitiveSearchableTab,
                            tab->GetTabAndroidWeakPtr())
                 .Then(barrier_cb));
  }
}

// static
static jlong JNI_AuxiliarySearchBridge_GetForProfile(JNIEnv* env,
                                                     Profile* profile) {
  DCHECK(profile);

  return reinterpret_cast<intptr_t>(
      AuxiliarySearchProviderFactory::GetForProfile(profile));
}

// static
void AuxiliarySearchProvider::EnsureFactoryBuilt() {
  AuxiliarySearchProviderFactory::GetInstance();
}

DEFINE_JNI(AuxiliarySearchBridge)
