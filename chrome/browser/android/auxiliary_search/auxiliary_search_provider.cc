// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/auxiliary_search/auxiliary_search_provider.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/AuxiliarySearchBridge_jni.h"
#include "chrome/browser/android/auxiliary_search/proto/auxiliary_search_group.pb.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "url/url_constants.h"

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
                .Build()) {}

 private:
  // ProfileKeyedServiceFactory overrides
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    DCHECK(!profile->IsOffTheRecord());

    return new AuxiliarySearchProvider(profile);
  }
};

void callJavaCallbackWithTabList(
    JNIEnv* env,
    const base::android::ScopedJavaGlobalRef<jobject>& j_callback_obj,
    std::unique_ptr<std::vector<base::WeakPtr<TabAndroid>>>
        non_sensitive_tabs) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>> j_tabs_list;
  for (base::WeakPtr<TabAndroid> tab_android : *non_sensitive_tabs.get()) {
    if (tab_android) {
      j_tabs_list.push_back(tab_android->GetJavaObject());
    }
  }
  base::android::RunObjectCallbackAndroid(
      j_callback_obj, base::android::ToJavaArrayOfObjects(env, j_tabs_list));
}

bool IsSchemeAllowed(const GURL& url) {
  return url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme);
}

void FilterNonSensitiveSearchableTabs(
    std::unique_ptr<std::vector<base::WeakPtr<TabAndroid>>> all_tabs,
    int current_tab_index,
    size_t max_tab_count,
    std::unique_ptr<std::vector<base::WeakPtr<TabAndroid>>> non_sensitive_tabs,
    BackToJavaCallback callback,
    PersistedTabDataAndroid* persisted_tab_data) {
  SensitivityPersistedTabDataAndroid* sensitivity_persisted_tab_data_android =
      static_cast<SensitivityPersistedTabDataAndroid*>(persisted_tab_data);

  if (current_tab_index >= 0 &&
      !sensitivity_persisted_tab_data_android->is_sensitive()) {
    non_sensitive_tabs->push_back(all_tabs->at(current_tab_index));
  }
  int next_tab_index = current_tab_index - 1;

  // Make sure the next tab index is valid, and the next tab's week pointer is
  // still available.
  while (next_tab_index >= 0 && max_tab_count > non_sensitive_tabs->size() &&
         !all_tabs->at(next_tab_index)) {
    --next_tab_index;
  }

  if (next_tab_index < 0 || max_tab_count <= non_sensitive_tabs->size()) {
    std::move(callback).Run(std::move(non_sensitive_tabs));
    return;
  }

  base::WeakPtr<TabAndroid> next_tab = all_tabs->at(next_tab_index);
  SensitivityPersistedTabDataAndroid::From(
      next_tab.get(),
      base::BindOnce(&FilterNonSensitiveSearchableTabs, std::move(all_tabs),
                     next_tab_index, max_tab_count,
                     std::move(non_sensitive_tabs), std::move(callback)));
}

}  // namespace

AuxiliarySearchProvider::AuxiliarySearchProvider(Profile* profile)
    : profile_(profile) {
  max_bookmark_donation_count_ =
      chrome::android::kAuxiliarySearchMaxBookmarksCountParam.Get();
  max_tab_donation_count_ =
      chrome::android::kAuxiliarySearchMaxTabsCountParam.Get();
}

AuxiliarySearchProvider::~AuxiliarySearchProvider() = default;

base::android::ScopedJavaLocalRef<jbyteArray>
AuxiliarySearchProvider::GetBookmarksSearchableData(JNIEnv* env) const {
  auxiliary_search::AuxiliarySearchBookmarkGroup group =
      GetBookmarks(BookmarkModelFactory::GetForBrowserContext(profile_.get()));

  std::string serialized_group;
  if (!group.SerializeToString(&serialized_group)) {
    serialized_group.clear();
  }

  return ToJavaByteArray(env, serialized_group);
}

base::android::ScopedJavaLocalRef<jobjectArray>
AuxiliarySearchProvider::GetSearchableTabs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& j_tabs_android) const {
  std::vector<TabAndroid*> all_tabs = TabAndroid::GetAllNativeTabs(
      env, base::android::ScopedJavaLocalRef(j_tabs_android));
  std::vector<base::WeakPtr<TabAndroid>> filtered_tabs =
      FilterTabsByScheme(all_tabs);

  std::vector<base::android::ScopedJavaLocalRef<jobject>> j_filtered_tabs;
  j_filtered_tabs.reserve(filtered_tabs.size());
  for (base::WeakPtr<TabAndroid> tab : filtered_tabs) {
    j_filtered_tabs.push_back(tab->GetJavaObject());
  }
  return base::android::ToJavaArrayOfObjects(env, j_filtered_tabs);
}

void AuxiliarySearchProvider::GetNonSensitiveTabs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& j_tabs_android,
    const base::android::JavaParamRef<jobject>& j_callback_obj) const {
  std::vector<TabAndroid*> all_tabs = TabAndroid::GetAllNativeTabs(
      env, base::android::ScopedJavaLocalRef(j_tabs_android));

  GetNonSensitiveTabsInternal(
      all_tabs, base::BindOnce(&callJavaCallbackWithTabList, env,
                               base::android::ScopedJavaGlobalRef<jobject>(
                                   j_callback_obj)));
}

auxiliary_search::AuxiliarySearchBookmarkGroup
AuxiliarySearchProvider::GetBookmarks(bookmarks::BookmarkModel* model) const {
  auxiliary_search::AuxiliarySearchBookmarkGroup group;
  std::vector<const BookmarkNode*> nodes;
  bookmarks::GetMostRecentlyUsedEntries(model, max_bookmark_donation_count_,
                                        &nodes);
  for (const BookmarkNode* node : nodes) {
    GURL url = node->url();
    if (!IsSchemeAllowed(url)) {
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
std::vector<base::WeakPtr<TabAndroid>>
AuxiliarySearchProvider::FilterTabsByScheme(
    const std::vector<TabAndroid*>& tabs) {
  std::vector<base::WeakPtr<TabAndroid>> filtered_tabs;
  for (TabAndroid* tab : tabs) {
    if (IsSchemeAllowed(tab->GetURL())) {
      filtered_tabs.push_back(tab->GetWeakPtr());
    }
  }
  return filtered_tabs;
}

void AuxiliarySearchProvider::GetNonSensitiveTabsInternal(
    const std::vector<TabAndroid*>& all_tabs,
    NonSensitiveTabsCallback callback) const {
  std::unique_ptr<std::vector<base::WeakPtr<TabAndroid>>> non_sensitive_tabs =
      std::make_unique<std::vector<base::WeakPtr<TabAndroid>>>();
  std::vector<base::WeakPtr<TabAndroid>> filtered_tabs =
      FilterTabsByScheme(all_tabs);
  if (filtered_tabs.size() == 0) {
    std::move(callback).Run(std::move(non_sensitive_tabs));
    return;
  }

  base::WeakPtr<TabAndroid> next_tab =
      filtered_tabs.at(filtered_tabs.size() - 1);
  SensitivityPersistedTabDataAndroid::From(
      next_tab.get(),
      base::BindOnce(&FilterNonSensitiveSearchableTabs,
                     std::make_unique<std::vector<base::WeakPtr<TabAndroid>>>(
                         filtered_tabs),
                     filtered_tabs.size() - 1, max_tab_donation_count_,
                     std::move(non_sensitive_tabs), std::move(callback)));
}

// static
jlong JNI_AuxiliarySearchBridge_GetForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);

  return reinterpret_cast<intptr_t>(
      AuxiliarySearchProviderFactory::GetForProfile(profile));
}

// static
void AuxiliarySearchProvider::EnsureFactoryBuilt() {
  AuxiliarySearchProviderFactory::GetInstance();
}
