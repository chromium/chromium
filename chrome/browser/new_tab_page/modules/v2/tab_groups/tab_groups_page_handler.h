// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_GROUPS_TAB_GROUPS_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_GROUPS_TAB_GROUPS_PAGE_HANDLER_H_

#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace tab_groups {
class SavedTabGroup;
class TabGroupId;
class TabGroupSyncService;
}  // namespace tab_groups

class TabGroupsPageHandler : public ntp::tab_groups::mojom::PageHandler {
 public:
  explicit TabGroupsPageHandler(
      mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>
          pending_page_handler,
      content::WebContents* web_contents);
  ~TabGroupsPageHandler() override;

  TabGroupsPageHandler(const TabGroupsPageHandler&) = delete;
  TabGroupsPageHandler& operator=(const TabGroupsPageHandler&) = delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // ntp::tab_groups::mojom::PageHandler:
  void CreateNewTabGroup() override;
  void GetTabGroups(GetTabGroupsCallback callback) override;
  void OpenTabGroup(const std::string& id) override;
  void DismissModule() override;
  void RestoreModule() override;

 private:
  bool ShouldShowZeroState();
  std::vector<const tab_groups::SavedTabGroup*> FilterActiveGroup(
      std::vector<const tab_groups::SavedTabGroup*> groups);
  std::vector<const tab_groups::SavedTabGroup*> GetMostRecentTabGroups(
      std::vector<const tab_groups::SavedTabGroup*> groups,
      size_t count);
  std::optional<std::string> GetDeviceName(
      const std::optional<std::string>& cache_guid);
  std::vector<ntp::tab_groups::mojom::TabGroupPtr> GetSavedTabGroups();
  void GetLastInteractedTimeForGroup(
      const std::optional<tab_groups::TabGroupId> group_id);

  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_service_;
  mojo::Receiver<ntp::tab_groups::mojom::PageHandler> page_handler_;

  base::WeakPtrFactory<TabGroupsPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_GROUPS_TAB_GROUPS_PAGE_HANDLER_H_
