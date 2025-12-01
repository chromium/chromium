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
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_state_storage_backend.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace tabs {

// Standardizes the underlying types backing the TabInterface to ensure
// consistent handles.
using TabCanonicalizer =
    base::RepeatingCallback<const TabInterface*(const TabInterface*)>;

// Constructs an associater using the specified callbacks. This indirection is
// required to minimize OS-specific coupling.
using RestoreEntityTrackerFactory = base::RepeatingCallback<std::unique_ptr<
    RestoreEntityTracker>(OnTabAssociation, OnCollectionAssociation)>;

class TabStateStorageService : public KeyedService,
                               public base::SupportsUserData,
                               public StorageIdMapping {
 public:
  using LoadDataCallback =
      base::OnceCallback<void(std::unique_ptr<StorageLoadedData>)>;

  TabStateStorageService(const base::FilePath& profile_path,
                         std::unique_ptr<TabStoragePackager> packager,
                         TabCanonicalizer tab_canonicalizer,
                         RestoreEntityTrackerFactory builder_factory);
  ~TabStateStorageService() override;

  // Boosts the priority of the database operations to USER_BLOCKING until all
  // current pending operations are complete. This should be used when it is
  // critical to save user data.
  void BoostPriority();

  // StorageIdMapping:
  StorageId GetStorageId(const TabCollection* collection) override;
  StorageId GetStorageId(const TabInterface* tab) override;

  // Schedules a task to be run after all currently pending operations are
  // complete. Primarily useful if you want to wait to ensure events prior to
  // calling this method are flushed to the database. There is no guarantee
  // that future operations will complete prior to the callback being invoked.
  void WaitForAllPendingOperations(base::OnceClosure on_idle);

  void Save(const TabInterface* tab);
  void Save(const TabCollection* collection);

  // This will silently fail if the collection has not already been saved to the
  // database.
  void SavePayload(const TabCollection* collection);

  void Move(const TabInterface* tab, const TabCollection* prev_parent);
  void Move(const TabCollection* collection, const TabCollection* prev_parent);

  void Remove(const TabInterface* tab, const TabCollection* prev_parent);
  void Remove(const TabCollection* collection,
              const TabCollection* prev_parent);

  void LoadAllNodes(const std::string& window_tag,
                    bool is_off_the_record,
                    LoadDataCallback callback);

  void ClearState();

  void ClearWindow(const std::string& window_tag);

  // Returns a Java object of the type TabStateStorageService. This is
  // implemented in tab_state_storage_service_android.cc
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      TabStateStorageService* tab_state_storage_service);

 private:
  void OnTabCreated(StorageId storage_id, const TabInterface* tab);
  void OnCollectionCreated(StorageId storage_id,
                           const TabCollection* collection);

  TabStateStorageBackend tab_backend_;
  std::unique_ptr<TabStoragePackager> packager_;

  TabCanonicalizer tab_canonicalizer_;
  RestoreEntityTrackerFactory tracker_factory_;

  // Storage ids need to be unique across tabs and collections, but the handles
  // do not have this guarantee. Track them separately.
  absl::flat_hash_map<int32_t, StorageId> tab_handle_to_storage_id_;
  absl::flat_hash_map<int32_t, StorageId> collection_handle_to_storage_id_;

  base::WeakPtrFactory<TabStateStorageService> weak_ptr_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_SERVICE_H_
