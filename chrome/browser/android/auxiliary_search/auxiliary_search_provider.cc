// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/auxiliary_search/auxiliary_search_provider.h"

#include <algorithm>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/auxiliary_search/proto/auxiliary_search_group.pb.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "url/url_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AuxiliarySearchBridge_jni.h"

using base::android::ToJavaByteArray;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

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
    DependsOn(BookmarkModelFactory::GetInstance());
  }

 private:
  // ProfileKeyedServiceFactory overrides
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    DCHECK(!profile->IsOffTheRecord());

    return new AuxiliarySearchProvider(
        BookmarkModelFactory::GetForBrowserContext(profile));
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

}  // namespace

AuxiliarySearchProvider::AuxiliarySearchProvider(
    bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {
}

AuxiliarySearchProvider::~AuxiliarySearchProvider() = default;

base::android::ScopedJavaLocalRef<jbyteArray>
AuxiliarySearchProvider::GetBookmarksSearchableData(JNIEnv* env) const {
  auxiliary_search::AuxiliarySearchBookmarkGroup group = GetBookmarks();

  std::string serialized_group;
  if (!group.SerializeToString(&serialized_group)) {
    serialized_group.clear();
  }

  return ToJavaByteArray(env, serialized_group);
}

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

auxiliary_search::AuxiliarySearchBookmarkGroup
AuxiliarySearchProvider::GetBookmarks() const {
  auxiliary_search::AuxiliarySearchBookmarkGroup group;
  std::vector<const BookmarkNode*> nodes;
  bookmarks::GetMostRecentlyUsedEntries(
      bookmark_model_,
      chrome::android::kAuxiliarySearchMaxBookmarksCountParam.Get(), &nodes);
  for (const BookmarkNode* node : nodes) {
    const GURL& url = node->url();
    if (!IsSchemeAllowed(node->url())) {
      continue;
    }

    auxiliary_search::AuxiliarySearchEntry* bookmark = group.add_bookmark();
    bookmark->set_title(base::UTF16ToUTF8(node->GetTitle()));
    bookmark->set_url(url.spec());
    if (!node->date_added().is_null()) {
      bookmark->set_creation_timestamp(
          node->date_added().InMillisecondsSinceUnixEpoch());
    }
    if (!node->date_last_used().is_null()) {
      bookmark->set_last_access_timestamp(
          node->date_last_used().InMillisecondsSinceUnixEpoch());
    }
  }

  return group;
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
