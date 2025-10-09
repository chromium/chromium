// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_SERVICE_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_SERVICE_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/android/token_android.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/tab_state_storage_backend.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace tabs_pb {
class TabState;
}  // namespace tabs_pb

namespace tabs {

class TabStateStorageService : public KeyedService,
                               public base::SupportsUserData,
                               public StorageIdMapping {
 public:
  using OnTabInterfaceCreation = base::OnceCallback<void(const TabInterface*)>;
  using LoadedTabState = std::pair<tabs_pb::TabState, OnTabInterfaceCreation>;
  using LoadAllTabsCallback =
      base::OnceCallback<void(std::vector<LoadedTabState>)>;

  explicit TabStateStorageService(
      std::unique_ptr<TabStateStorageBackend> tab_backend,
      std::unique_ptr<TabStoragePackager>);
  ~TabStateStorageService() override;

  // StorageIdMapping:
  int GetStorageId(const TabCollection* collection) override;
  int GetStorageId(const TabInterface* tab) override;

  void Save(const TabInterface* tab);
  void Save(const TabCollection* collection);

  void LoadAllTabs(LoadAllTabsCallback callback);

  // Returns a Java object of the type TabStateStorageService. This is
  // implemented in tab_state_storage_service_android.cc
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      TabStateStorageService* tab_state_storage_service);

 private:
  void OnAllTabsLoaded(LoadAllTabsCallback callback,
                       std::vector<NodeState> entries);

  void OnTabCreated(int storage_id, const TabInterface* tab);

  std::unique_ptr<TabStateStorageBackend> tab_backend_;
  std::unique_ptr<TabStoragePackager> packager_;

  // Storage ids need to be unique across tabs and collections, but the handles
  // do not have this guarantee. Track them separately.
  int next_storage_id_ = 1;
  absl::flat_hash_map<int32_t, int> tab_handle_to_storage_id_;
  absl::flat_hash_map<int32_t, int> collection_handle_to_storage_id_;

  base::WeakPtrFactory<TabStateStorageService> weak_ptr_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_SERVICE_H_
