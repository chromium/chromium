// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"

namespace signin_ui_util {

void SigninUiDelegate::ShowTurnSyncOnUI(
    Profile* profile,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const CoreAccountId& account_id,
    TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode,
    bool is_sync_promo,
    bool user_already_signed_in) {
  // TODO(crbug.com/417950948): Delete this function when removing the Sync
  // feature.
  CHECK(!base::FeatureList::IsEnabled(
      syncer::kReplaceSyncPromosWithSignInPromos));
  // TurnSyncOnHelper is suicidal (it will delete itself once it finishes
  // enabling sync).
  new TurnSyncOnHelper(profile, EnsureBrowser(profile), access_point,
                       promo_action, account_id, signin_aborted_mode,
                       is_sync_promo, user_already_signed_in);
}

void SigninUiDelegate::ShowHistorySyncOptinUI(
    Profile* profile,
    const CoreAccountId& account_id,
    signin_metrics::AccessPoint access_point) {
  CHECK(
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));
  HistorySyncOptinService* history_sync_optin_service =
      HistorySyncOptinServiceFactory::GetForProfile(profile);
  CHECK(history_sync_optin_service);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
  auto extended_account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);

  history_sync_optin_service->StartHistorySyncOptinFlow(
      extended_account_info,
      std::make_unique<HistorySyncOptinServiceDefaultDelegate>(), access_point);
}

// static
Browser* SigninUiDelegate::EnsureBrowser(Profile* profile) {
  DCHECK(profile);
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  return displayer.browser();
}

}  // namespace signin_ui_util
