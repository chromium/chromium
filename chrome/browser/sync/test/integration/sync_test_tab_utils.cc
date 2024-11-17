// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test_tab_utils.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif  // BUILDFLAG(IS_ANDROID)

using sync_datatype_helper::test;

namespace sync_test_tab_utils {

namespace {

#if BUILDFLAG(IS_ANDROID)

TabModel* GetActiveTabModel() {
  for (TabModel* model : TabModelList::models()) {
    if (model->IsActiveModel()) {
      return model;
    }
  }
  NOTREACHED() << "No active tab model";
}

TabAndroid* GetFirstTabAtGroup(
    const tab_groups::LocalTabGroupID& local_group_id) {
  TabModel* active_model = GetActiveTabModel();
  for (int i = 0; i < active_model->GetTabCount(); ++i) {
    TabAndroid* tab = active_model->GetTabAt(i);
    CHECK(tab);
    if (sync_test_utils_android::GetGroupIdForTab(tab) == local_group_id) {
      return tab;
    }
  }
  return nullptr;
}

#else  // BUILDFLAG(IS_ANDROID)

Browser* GetBrowserOrDie() {
  CHECK_EQ(test()->num_clients(), 1)
      << "Tab utils support only single-client tests";
  Browser* browser = test()->GetBrowser(/*index=*/0);
  CHECK(browser);
  return browser;
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

std::optional<size_t> OpenNewTab(const GURL& url) {
  content::WebContents* web_contents = nullptr;
  size_t tab_index = 0;
#if BUILDFLAG(IS_ANDROID)
  tab_index = GetActiveTabModel()->GetTabCount();
  web_contents = GetActiveTabModel()->CreateNewTabForDevTools(url);
#else
  TabStripModel* tab_strip = GetBrowserOrDie()->tab_strip_model();
  tab_index = tab_strip->count();
  web_contents = chrome::AddAndReturnTabAt(GetBrowserOrDie(), url, tab_index,
                                           /*foreground=*/true);
#endif  // BUILDFLAG(IS_ANDROID)
  CHECK(web_contents);
  CHECK(WaitForLoadStop(web_contents)) << "Failed to load URL: " << url;
  return tab_index;
}

tab_groups::LocalTabGroupID CreateGroupFromTab(
    size_t tab_index,
    std::string_view title,
    tab_groups::TabGroupColorId color) {
  std::optional<tab_groups::LocalTabGroupID> local_group_id;
#if BUILDFLAG(IS_ANDROID)
  TabAndroid* tab = GetActiveTabModel()->GetTabAt(tab_index);
  CHECK(tab);
  local_group_id = sync_test_utils_android::CreateGroupFromTab(tab);
#else
  TabStripModel* tab_strip = GetBrowserOrDie()->tab_strip_model();
  local_group_id = tab_strip->AddToNewGroup({static_cast<int>(tab_index)});
#endif  // BUILDFLAG(IS_ANDROID)
  CHECK(local_group_id.has_value());
  UpdateTabGroupVisualData(local_group_id.value(), title, color);
  return local_group_id.value();
}

bool IsTabGroupOpen(const tab_groups::LocalTabGroupID& local_group_id) {
#if BUILDFLAG(IS_ANDROID)
  return GetFirstTabAtGroup(local_group_id) != nullptr;
#else
  TabStripModel* tab_strip = GetBrowserOrDie()->tab_strip_model();
  return tab_strip->group_model()->ContainsTabGroup(local_group_id);
#endif  // BUILDFLAG(IS_ANDROID)
}

void UpdateTabGroupVisualData(const tab_groups::LocalTabGroupID& local_group_id,
                              const std::string_view& title,
                              tab_groups::TabGroupColorId color) {
#if BUILDFLAG(IS_ANDROID)
  return sync_test_utils_android::UpdateTabGroupVisualData(
      GetFirstTabAtGroup(local_group_id), title, color);
#else
  TabStripModel* tab_strip = GetBrowserOrDie()->tab_strip_model();
  TabGroup* model_tab_group =
      tab_strip->group_model()->GetTabGroup(local_group_id);
  CHECK(model_tab_group);
  model_tab_group->SetVisualData(
      tab_groups::TabGroupVisualData(base::UTF8ToUTF16(title), color),
      /*is_customized=*/true);
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace sync_test_tab_utils
