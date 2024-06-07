// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_session_tab_helper.h"

#include <optional>
#include <string>

#include "chrome/browser/ash/growth/campaigns_manager_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

CampaignsManagerSessionTabHelper::~CampaignsManagerSessionTabHelper() = default;

void CampaignsManagerSessionTabHelper::PrimaryPageChanged(content::Page& page) {
  auto* session = CampaignsManagerSession::Get();
  if (!session) {
    return;
  }

  session->PrimaryPageChanged(web_contents());
}

CampaignsManagerSessionTabHelper::CampaignsManagerSessionTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CampaignsManagerSessionTabHelper>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CampaignsManagerSessionTabHelper);
