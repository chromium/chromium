// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_TAB_HELPER_H_
#define CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_TAB_HELPER_H_

#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

class CampaignsManagerSessionTabHelper : public content::WebContentsObserver {
 public:
  explicit CampaignsManagerSessionTabHelper(content::WebContents* web_contents);
  CampaignsManagerSessionTabHelper(const CampaignsManagerSessionTabHelper&) =
      delete;
  CampaignsManagerSessionTabHelper& operator=(
      const CampaignsManagerSessionTabHelper&) = delete;

  ~CampaignsManagerSessionTabHelper() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
};

#endif  // CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_TAB_HELPER_H_
