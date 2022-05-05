// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

bool ShouldOfferSignin(Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed) &&
         sync_service && sync_service->GetAccountInfo().IsEmpty() &&
         !sync_service->IsLocalSyncEnabled();
}

}  // namespace

absl::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    content::WebContents* web_contents) {
  if (!web_contents)
    return absl::nullopt;

  if (!web_contents->GetURL().SchemeIsHTTPOrHTTPS())
    return absl::nullopt;

  if (!web_contents->GetController().GetLastCommittedEntry())
    return absl::nullopt;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  SendTabToSelfSyncService* send_tab_to_self_sync_service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  if (!send_tab_to_self_sync_service) {
    // Can happen in incognito or guest profile.
    return absl::nullopt;
  }

  if (ShouldOfferSignin(profile) &&
      base::FeatureList::IsEnabled(kSendTabToSelfSigninPromo)) {
    return EntryPointDisplayReason::kOfferSignIn;
  }

  SendTabToSelfModel* model =
      send_tab_to_self_sync_service->GetSendTabToSelfModel();
  if (!model->IsReady())
    return absl::nullopt;

  if (!model->HasValidTargetDevice()) {
    return base::FeatureList::IsEnabled(kSendTabToSelfSigninPromo)
               ? absl::make_optional(
                     EntryPointDisplayReason::kInformNoTargetDevice)
               : absl::nullopt;
  }

  return EntryPointDisplayReason::kOfferFeature;
}

bool ShouldDisplayEntryPoint(content::WebContents* web_contents) {
  return GetEntryPointDisplayReason(web_contents).has_value();
}

bool ShouldOfferOmniboxIcon(content::WebContents* web_contents) {
  if (!web_contents)
    return false;
  return !web_contents->IsWaitingForResponse() &&
         ShouldDisplayEntryPoint(web_contents);
}

}  // namespace send_tab_to_self
