// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace auxiliary_search {
class AuxiliarySearchBookmarkGroup;
}
namespace bookmarks {
class BookmarkModel;
}
class TabAndroid;

// AuxiliarySearchProvider is responsible for providing the necessary
// information for the auxiliary search..
class AuxiliarySearchProvider : public KeyedService {
 public:
  // DO NOT pass a Profile here, keyed services must have explicit dependencies
  // on other keyed services (crbug.com/368297674).
  explicit AuxiliarySearchProvider(bookmarks::BookmarkModel* bookmark_model);
  ~AuxiliarySearchProvider() override;

  base::android::ScopedJavaLocalRef<jbyteArray> GetBookmarksSearchableData(
      JNIEnv* env) const;

  void GetNonSensitiveTabs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& j_tabs_android,
      const base::android::JavaParamRef<jobject>& j_callback_obj) const;

  static void EnsureFactoryBuilt();

 private:
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderTest, QueryBookmarks);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderTest,
                           QueryBookmarks_flagTest);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderTest,
                           QueryBookmarks_nativePageShouldBeFiltered);
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

  auxiliary_search::AuxiliarySearchBookmarkGroup GetBookmarks() const;

  static void FilterTabsByScheme(
      std::vector<raw_ptr<TabAndroid, VectorExperimental>>& tabs);

  void GetNonSensitiveTabsInternal(
      std::vector<raw_ptr<TabAndroid, VectorExperimental>> all_tabs,
      NonSensitiveTabsCallback callback) const;

  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

#endif  // CHROME_BROWSER_ANDROID_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_
