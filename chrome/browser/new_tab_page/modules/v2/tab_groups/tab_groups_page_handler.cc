// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups_page_handler.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"

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
    const int kSampleTabGroupsCount = 4;
    for (int i = 0; i < kSampleTabGroupsCount; ++i) {
      auto tab_group = ntp::tab_groups::mojom::TabGroup::New();
      tab_group->title = "Tab Group " + base::NumberToString(i + 1);
      tab_group->url = GURL("https://www.google.com");
      tab_groups_mojom.push_back(std::move(tab_group));
    }
  }

  std::move(callback).Run(std::move(tab_groups_mojom));
}
