// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_GROUPS_TAB_GROUPS_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_GROUPS_TAB_GROUPS_PAGE_HANDLER_H_

#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

class TabGroupsPageHandler : public ntp::tab_groups::mojom::PageHandler {
 public:
  explicit TabGroupsPageHandler(
      mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>
          pending_page_handler,
      content::WebContents* web_contents);
  ~TabGroupsPageHandler() override;

  TabGroupsPageHandler(const TabGroupsPageHandler&) = delete;
  TabGroupsPageHandler& operator=(const TabGroupsPageHandler&) = delete;

  // ntp::tab_groups::mojom::PageHandler:
  void GetTabGroups(GetTabGroupsCallback callback) override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

  mojo::Receiver<ntp::tab_groups::mojom::PageHandler> page_handler_;

  base::WeakPtrFactory<TabGroupsPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_GROUPS_TAB_GROUPS_PAGE_HANDLER_H_
