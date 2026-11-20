// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace visited_url_ranking {
class VisitedURLRankingService;
}  // namespace visited_url_ranking

class GURL;
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
  // The maximum number of tabs donated to the auxiliary search.
  static constexpr size_t kMaxDonatedTabsCount = 100;

  AuxiliarySearchProvider(
      visited_url_ranking::VisitedURLRankingService* ranking_service);

  ~AuxiliarySearchProvider() override;

  void GetNonSensitiveTabs(
      JNIEnv* env,
      std::vector<TabAndroid*> tabs,
      const base::android::JavaRef<jobject>& j_callback_obj) const;

  void GetNonSensitiveHistoryData(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_callback_obj) const;

  // Fetches CCTs after the given begin time from the history database.
  void GetCustomTabs(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_url,
      int64_t j_begin_time,
      const base::android::JavaRef<jobject>& j_callback_obj) const;

  // The visit IDs below are non-negative 63-bit values that are stable across
  // restarts, so donating the same entry again replaces its previous document.

  // Computes the visit ID of a Custom Tab entry from the `url_key` of its URL
  // visit aggregate.
  static int64_t GetCustomTabVisitId(std::string_view url_key);

  // Computes the visit ID of a top site entry from its `url` and `title`.
  static int64_t GetTopSiteVisitId(const GURL& url,
                                   const std::u16string& title);

  static void EnsureFactoryBuilt();

 private:
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest,
                           QuerySensitiveTab);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest,
                           QueryNonSensitiveTab);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest,
                           QueryNonSensitiveTab_CappedByMaxCount);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest,
                           QueryEmptyTabList);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest, NativeTabTest);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderBrowserTest, FilterTabsTest);

  using NonSensitiveTabsCallback =
      base::OnceCallback<void(std::vector<base::WeakPtr<TabAndroid>>)>;

  static void FilterTabsByScheme(std::vector<TabAndroid*>& tabs);

  // Returns at most `max_donated_tabs_count` non-sensitive tabs from
  // `all_tabs` via `callback`.
  void GetNonSensitiveTabsInternal(std::vector<TabAndroid*> all_tabs,
                                   size_t max_donated_tabs_count,
                                   NonSensitiveTabsCallback callback) const;

  const raw_ptr<visited_url_ranking::VisitedURLRankingService> ranking_service_;
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_
