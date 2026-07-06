// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_delegate_impl_android.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace signin_ui_util {
namespace {

content::WebContents* GetActiveWebContents(Profile* profile) {
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() == profile &&
        tab_model->GetActiveWebContents()) {
      return tab_model->GetActiveWebContents();
    }
  }
  return nullptr;
}

}  // namespace

void SigninUiDelegateImplAndroid::ShowSigninUI(
    Profile* profile,
    bool enable_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& extension_name) {
  CHECK(profile);
  CHECK(!extension_name.empty())
      << "SigninUiDelegate is only used for extension flows on Android";

  content::WebContents* web_contents = GetActiveWebContents(profile);
  if (!web_contents) {
    // TODO(crbug.com/403867715): Open a new tab in this case.
    return;
  }

  std::vector<CoreAccountInfo> accounts =
      IdentityManagerFactory::GetForProfile(profile)
          ->GetAccountsWithRefreshTokens();
  if (accounts.empty()) {
    SigninBridgeFactory::GetForProfile(profile)->StartAddAccountFlow(
        TabAndroid::FromWebContents(web_contents),
        /*prefilled_email= */ std::string(),
        /*continue_url=*/web_contents->GetLastCommittedURL(), extension_name);
    return;
  }

  SigninBridgeFactory::GetForProfile(profile)
      ->OpenAccountPickerBottomSheetForExtensions(
          web_contents,
          /*continue_url=*/web_contents->GetLastCommittedURL(),
          accounts.front().account_id, extension_name);
}

void SigninUiDelegateImplAndroid::ShowReauthUI(
    Profile* profile,
    const std::string& email,
    bool enable_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action) {
  CHECK(profile);
  CHECK(profile->IsRegularProfile());

  content::WebContents* web_contents = GetActiveWebContents(profile);
  if (!web_contents) {
    // TODO(crbug.com/403867715): Open a new tab in this case.
    return;
  }
  CoreAccountId account_id = IdentityManagerFactory::GetForProfile(profile)
                                 ->FindExtendedAccountInfoByEmailAddress(email)
                                 .account_id;
  CHECK(!account_id.empty());
  SigninBridgeFactory::GetForProfile(profile)->StartUpdateCredentialsFlow(
      TabAndroid::FromWebContents(web_contents),
      /*continue_url=*/web_contents->GetLastCommittedURL(), account_id);
}

}  // namespace signin_ui_util
