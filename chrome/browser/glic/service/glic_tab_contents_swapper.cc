// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_tab_contents_swapper.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"

namespace glic {

namespace {

struct TabLocation {
  raw_ptr<TabListInterface> tab_list;
  int index;
};

std::optional<TabLocation> GetTabLocation(tabs::TabInterface* tab) {
  auto* browser = tab->GetBrowserWindowInterface();
  if (!browser) {
    return std::nullopt;
  }
  auto* tab_list = TabListInterface::From(browser);
  if (!tab_list) {
    return std::nullopt;
  }
  int index = tab_list->GetIndexOfTab(tab->GetHandle());
  if (index == -1) {
    return std::nullopt;
  }
  return TabLocation{tab_list, index};
}

}  // namespace

tabs::TabInterface* SwapGlicTabToPlaceholder(
    tabs::TabInterface* real_tab,
    base::OnceCallback<void(std::unique_ptr<content::WebContents>)>
        reclaim_callback) {
  auto location = GetTabLocation(real_tab);
  if (!location) {
    return nullptr;
  }
  auto [tab_list, index] = *location;

  Profile* profile =
      Profile::FromBrowserContext(real_tab->GetContents()->GetBrowserContext());
  std::optional<tab_groups::TabGroupId> group_id = real_tab->GetGroup();

  std::unique_ptr<content::WebContents> real_contents =
      tab_list->DetachWebContents(real_tab->GetHandle());
  if (!real_contents) {
    return nullptr;
  }

  std::u16string original_title = real_contents->GetTitle();

  std::unique_ptr<content::WebContents> placeholder =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  placeholder->SetUserData(GlicPlaceholderUserData::kKey,
                           std::make_unique<GlicPlaceholderUserData>());

  std::unique_ptr<content::NavigationEntry> nav_entry =
      content::NavigationEntry::Create();
  nav_entry->SetURL(GURL("about:blank"));
  nav_entry->SetVirtualURL(GURL(chrome::kChromeUIGlicURL));
  nav_entry->SetTitle(original_title);
  content::FaviconStatus& favicon = nav_entry->GetFavicon();
  favicon.valid = true;
  favicon.url = GURL(base::StrCat({chrome::kChromeUIGlicURL, "favicon"}));
  favicon.image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_GLIC_BUTTON_ALT_ICON);

  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.push_back(std::move(nav_entry));
  placeholder->GetController().Restore(0, content::RestoreType::kRestored,
                                       &entries);

  tabs::TabInterface* placeholder_tab = tab_list->InsertWebContentsAt(
      index, std::move(placeholder), /*should_pin=*/false, group_id);

  std::move(reclaim_callback).Run(std::move(real_contents));

  return placeholder_tab;
}

tabs::TabInterface* SwapPlaceholderToGlic(
    tabs::TabInterface* placeholder_tab,
    std::unique_ptr<content::WebContents> real_contents,
    std::optional<tab_groups::TabGroupId> tab_group_id) {
  auto location = GetTabLocation(placeholder_tab);
  if (!location) {
    return nullptr;
  }
  auto [tab_list, index] = *location;

  tab_list->DetachWebContents(placeholder_tab->GetHandle());

  return tab_list->InsertWebContentsAt(index, std::move(real_contents),
                                       /*should_pin=*/false, tab_group_id);
}

}  // namespace glic
