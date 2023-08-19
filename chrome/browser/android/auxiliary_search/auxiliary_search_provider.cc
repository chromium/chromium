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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"

using base::android::ToJavaByteArray;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

const size_t kMaxBookmarksCount = 100u;
const size_t kMaxTabsCount = 100u;

using BackToJavaCallback =
    base::OnceCallback<void(std::unique_ptr<std::vector<TabAndroid*>>)>;

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
    std::unique_ptr<std::vector<TabAndroid*>> non_sensitive_tabs) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>> j_tabs_list;
  for (TabAndroid* tab_android : *non_sensitive_tabs.get()) {
    j_tabs_list.push_back(tab_android->GetJavaObject());
  }
  base::android::RunObjectCallbackAndroid(
      j_callback_obj, base::android::ToJavaArrayOfObjects(env, j_tabs_list));
}

void FilterNonSensitiveTabs(
    std::unique_ptr<std::vector<TabAndroid*>> all_tabs,
    int current_tab_index,
    std::unique_ptr<std::vector<TabAndroid*>> non_sensitive_tabs,
    BackToJavaCallback callback,
    PersistedTabDataAndroid* persisted_tab_data) {
  SensitivityPersistedTabDataAndroid* sensitivity_persisted_tab_data_android =
      static_cast<SensitivityPersistedTabDataAndroid*>(persisted_tab_data);

  if (current_tab_index >= 0 &&
      !sensitivity_persisted_tab_data_android->is_sensitive()) {
    non_sensitive_tabs->push_back(all_tabs->at(current_tab_index));
  }
  int next_tab_index = current_tab_index - 1;

  if (next_tab_index < 0 || kMaxTabsCount <= non_sensitive_tabs->size()) {
    std::move(callback).Run(std::move(non_sensitive_tabs));
    return;
  }

  TabAndroid* next_tab = all_tabs->at(next_tab_index);
  SensitivityPersistedTabDataAndroid::From(
      next_tab, base::BindOnce(&FilterNonSensitiveTabs, std::move(all_tabs),
                               next_tab_index, std::move(non_sensitive_tabs),
                               std::move(callback)));
}

}  // namespace

AuxiliarySearchProvider::AuxiliarySearchProvider(Profile* profile)
    : profile_(profile) {}

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

void AuxiliarySearchProvider::GetNonSensitiveTabs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& j_tabs_android,
    const base::android::JavaParamRef<jobject>& j_callback_obj) const {
  std::vector<TabAndroid*> all_tabs = TabAndroid::GetAllNativeTabs(
      env, base::android::ScopedJavaLocalRef(j_tabs_android));

  GetNonSensitiveTabsInternal(
      std::make_unique<std::vector<TabAndroid*>>(all_tabs),
      base::BindOnce(
          &callJavaCallbackWithTabList, env,
          base::android::ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

auxiliary_search::AuxiliarySearchBookmarkGroup
AuxiliarySearchProvider::GetBookmarks(bookmarks::BookmarkModel* model) const {
  auxiliary_search::AuxiliarySearchBookmarkGroup group;
  std::vector<const BookmarkNode*> nodes;
  bookmarks::GetMostRecentlyUsedEntries(model, kMaxBookmarksCount, &nodes);
  for (const BookmarkNode* node : nodes) {
    auxiliary_search::AuxiliarySearchEntry* bookmark = group.add_bookmark();
    bookmark->set_title(base::UTF16ToUTF8(node->GetTitle()));
    bookmark->set_url(node->url().spec());
    if (!node->date_added().is_null()) {
      bookmark->set_creation_timestamp(node->date_added().ToJavaTime());
    }
    if (!node->date_last_used().is_null()) {
      bookmark->set_last_access_timestamp(node->date_last_used().ToJavaTime());
    }
  }

  return group;
}

void AuxiliarySearchProvider::GetNonSensitiveTabsInternal(
    std::unique_ptr<std::vector<TabAndroid*>> all_tabs,
    NonSensitiveTabsCallback callback) const {
  std::unique_ptr<std::vector<TabAndroid*>> non_sensitive_tabs =
      std::make_unique<std::vector<TabAndroid*>>();
  if (all_tabs->size() == 0) {
    std::move(callback).Run(std::move(non_sensitive_tabs));
    return;
  }

  TabAndroid* next_tab = all_tabs->at(all_tabs->size() - 1);
  SensitivityPersistedTabDataAndroid::From(
      next_tab,
      base::BindOnce(&FilterNonSensitiveTabs, std::move(all_tabs),
                     all_tabs->size() - 1, std::move(non_sensitive_tabs),
                     std::move(callback)));
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
