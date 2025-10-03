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
#include "base/supports_user_data.h"
#include "chrome/browser/tab/tab_state_storage_backend.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs_pb {
class TabState;
}  // namespace tabs_pb

namespace tabs {

class TabStateStorageService : public KeyedService,
                               public base::SupportsUserData {
 public:
  using LoadAllTabsCallback =
      base::OnceCallback<void(std::vector<tabs_pb::TabState>)>;

  explicit TabStateStorageService(
      std::unique_ptr<TabStateStorageBackend> tab_backend,
      std::unique_ptr<TabStoragePackager>);
  ~TabStateStorageService() override;

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
  std::unique_ptr<TabStateStorageBackend> tab_backend_;
  std::unique_ptr<TabStoragePackager> packager_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_SERVICE_H_
