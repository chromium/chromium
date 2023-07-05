// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "components/keyed_service/core/keyed_service.h"

namespace auxiliary_search {
class AuxiliarySearchBookmarkGroup;
class AuxiliarySearchTabGroup;
}
namespace bookmarks {
class BookmarkModel;
}
class Profile;

// AuxiliarySearchProvider is responsible for providing the necessary
// information for the auxiliary search..
class AuxiliarySearchProvider : public KeyedService {
 public:
  explicit AuxiliarySearchProvider(Profile* profile);
  ~AuxiliarySearchProvider() override;

  base::android::ScopedJavaLocalRef<jbyteArray> GetBookmarksSearchableData(
      JNIEnv* env) const;

  base::android::ScopedJavaLocalRef<jbyteArray> GetTabsSearchableData(
      JNIEnv* env) const;

  static void EnsureFactoryBuilt();

 private:
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderTest, QueryBookmarks);
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchProviderTest, QueryTabs);

  auxiliary_search::AuxiliarySearchBookmarkGroup GetBookmarks(
      bookmarks::BookmarkModel* model) const;

  auxiliary_search::AuxiliarySearchTabGroup GetTabs() const;

  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_ANDROID_AUXILIARY_SEARCH_AUXILIARY_SEARCH_PROVIDER_H_
