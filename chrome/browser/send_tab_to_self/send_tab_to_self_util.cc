// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace send_tab_to_self {

std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    content::WebContents* web_contents) {
  if (!web_contents)
    return std::nullopt;

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  return service ? service->GetEntryPointDisplayReason(
                       web_contents->GetLastCommittedURL())
                 : std::nullopt;
}

bool ShouldDisplayEntryPoint(content::WebContents* web_contents) {
  return GetEntryPointDisplayReason(web_contents).has_value();
}

}  // namespace send_tab_to_self
