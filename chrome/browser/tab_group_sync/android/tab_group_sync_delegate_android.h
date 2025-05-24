// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_GROUP_SYNC_ANDROID_TAB_GROUP_SYNC_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_TAB_GROUP_SYNC_ANDROID_TAB_GROUP_SYNC_DELEGATE_ANDROID_H_

#include <map>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace tab_groups {
class TabGroupSyncService;

// Android implementation of TabGroupSyncDelegate. Owns the Java object and
// passes all the calls to it which hosts all the interaction logic.
class TabGroupSyncDelegateAndroid : public TabGroupSyncDelegate {
 public:
  explicit TabGroupSyncDelegateAndroid(
      TabGroupSyncService* service,
      ScopedJavaLocalRef<jobject> j_delegate_deps);
  ~TabGroupSyncDelegateAndroid() override;

  // TabGroupSyncDelegate implementation.
  std::optional<LocalTabGroupID> HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) override;
  void CreateLocalTabGroup(const SavedTabGroup& tab_group) override;
  void CloseLocalTabGroup(const LocalTabGroupID& local_id) override;
  void ConnectLocalTabGroup(const SavedTabGroup& group) override;
  void DisconnectLocalTabGroup(const LocalTabGroupID& local_id) override;
  void UpdateLocalTabGroup(const SavedTabGroup& group) override;
  std::vector<LocalTabGroupID> GetLocalTabGroupIds() override;
  std::vector<LocalTabID> GetLocalTabIdsForTabGroup(
      const LocalTabGroupID& local_tab_group_id) override;
  std::set<LocalTabID> GetSelectedTabs() override;
  std::u16string GetTabTitle(const LocalTabID& local_tab_id) override;
  std::unique_ptr<SavedTabGroup> CreateSavedTabGroupFromLocalGroup(
      const LocalTabGroupID& local_tab_group_id) override;
  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() override;

 private:
  // A reference to the Java counterpart of this class. See
  // TabGroupSyncServiceDelegate.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned. This is safe because the delegate is owned by the service which
  // outlives the delegate.
  raw_ptr<TabGroupSyncService> tab_group_sync_service_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_TAB_GROUP_SYNC_ANDROID_TAB_GROUP_SYNC_DELEGATE_ANDROID_H_
