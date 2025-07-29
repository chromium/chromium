// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups_page_handler.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"

namespace {

ntp::tab_groups::mojom::TabGroupPtr MakeTabGroup(
    const char* title,
    std::initializer_list<const char*> urls,
    int total_tabs) {
  auto group = ntp::tab_groups::mojom::TabGroup::New();
  group->title = title;
  group->favicon_urls.reserve(urls.size());
  for (const char* url : urls) {
    group->favicon_urls.emplace_back(url);
  }
  group->total_tab_count = total_tabs;
  return group;
}

}  // namespace

TabGroupsPageHandler::TabGroupsPageHandler(
    mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>
        pending_page_handler,
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  DCHECK(web_contents_);
}

TabGroupsPageHandler::~TabGroupsPageHandler() = default;

void TabGroupsPageHandler::GetTabGroups(GetTabGroupsCallback callback) {
  const std::string data_type_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpTabGroupsModule,
      ntp_features::kNtpTabGroupsModuleDataParam);
  std::vector<ntp::tab_groups::mojom::TabGroupPtr> tab_groups_mojom;

  if (data_type_param.find("Fake Data") != std::string::npos) {
    // Generate fake data.
    tab_groups_mojom.push_back(
        MakeTabGroup("Tab Group 1 (3 tabs total)",
                     {"https://www.google.com", "https://www.youtube.com",
                      "https://www.wikipedia.org"},
                     3));

    tab_groups_mojom.push_back(
        MakeTabGroup("Tab Group 2 (4 tabs total)",
                     {"https://www.google.com", "https://www.youtube.com",
                      "https://www.wikipedia.org", "https://maps.google.com"},
                     4));

    tab_groups_mojom.push_back(
        MakeTabGroup("Tab Group 3 (8 tabs total)",
                     {"https://www.google.com", "https://www.youtube.com",
                      "https://www.wikipedia.org", "https://maps.google.com"},
                     8));

    tab_groups_mojom.push_back(
        MakeTabGroup("Tab Group 4 (199 tabs total)",
                     {"https://www.google.com", "https://www.youtube.com",
                      "https://www.wikipedia.org", "https://maps.google.com"},
                     199));
  } else if (data_type_param.find("Fake Zero State") != std::string::npos) {
    // No-op: the zero state card only appears when there's no data.
  }

  std::move(callback).Run(std::move(tab_groups_mojom));
}
