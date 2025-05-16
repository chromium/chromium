// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace visited_url_ranking {
class VisitedURLRankingService;
}  // namespace visited_url_ranking

class TabAndroid;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.auxiliary_search
enum class AuxiliarySearchEntryType {
  kTab = 0,
  kCustomTab = 1,
  kTopSite = 2,
  // New values above this line.
  kMaxValue = kTopSite
};

// AuxiliarySearchProvider is responsible for providing the necessary
// information for the auxiliary search.
class AuxiliarySearchProvider : public KeyedService {
 public:
  AuxiliarySearchProvider(
      visited_url_ranking::VisitedURLRankingService* ranking_service);

  ~AuxiliarySearchProvider() override;

  void GetNonSensitiveTabs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& j_tabs_android,
      const base::android::JavaParamRef<jobject>& j_callback_obj) const;

  void GetNonSensitiveHistoryData(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_callback_obj) const;

  // Fetches CCTs after the given begin time from the history database.
  void GetCustomTabs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_url,
      jlong j_begin_time,
      const base::android::JavaParamRef<jobject>& j_callback_obj) const;

  static void EnsureFactoryBuilt();

 private:
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest,
                           QuerySensitiveTab);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest,
                           QueryNonSensitiveTab);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest,
                           QueryNonSensitiveTab_flagTest);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest,
                           QueryEmptyTabList);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest, NativeTabTest);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest, FilterTabsTest);

  using NonSensitiveTabsCallback =
      base::OnceCallback<void(std::vector<base::WeakPtr<TabAndroid>>)>;

  static void FilterTabsByScheme(
      std::vector<raw_ptr<TabAndroid, VectorExperimental>>& tabs);

  void GetNonSensitiveTabsInternal(
      std::vector<raw_ptr<TabAndroid, VectorExperimental>> all_tabs,
      NonSensitiveTabsCallback callback) const;

  const raw_ptr<visited_url_ranking::VisitedURLRankingService> ranking_service_;
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_
