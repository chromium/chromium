// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace send_tab_to_self {

bool ShouldOfferFeature(content::WebContents* web_contents) {
  if (!web_contents)
    return false;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);

  GURL page_url = web_contents->GetURL();
  content::NavigationEntry* navigation_entry =
      web_contents->GetController().GetLastCommittedEntry();
  bool has_last_entry = (navigation_entry != nullptr);

  return ShouldOfferToShareUrl(service, page_url) && has_last_entry;
}

bool ShouldOfferToShareUrl(
    SendTabToSelfSyncService* send_tab_to_self_sync_service,
    const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return false;

  if (!send_tab_to_self_sync_service) {
    // Can happen in incognito or guest profile.
    return false;
  }

  SendTabToSelfModel* model =
      send_tab_to_self_sync_service->GetSendTabToSelfModel();
  return model->IsReady() && model->HasValidTargetDevice();
}

bool ShouldOfferOmniboxIcon(content::WebContents* web_contents) {
  if (!web_contents)
    return false;
  return !web_contents->IsWaitingForResponse() &&
         ShouldOfferFeature(web_contents);
}

}  // namespace send_tab_to_self
