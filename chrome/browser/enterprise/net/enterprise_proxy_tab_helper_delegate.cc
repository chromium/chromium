// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/enterprise_proxy_tab_helper_delegate.h"

#include "build/build_config.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif

namespace enterprise_net {

EnterpriseProxyTabHelperDelegate::EnterpriseProxyTabHelperDelegate() = default;

EnterpriseProxyTabHelperDelegate::~EnterpriseProxyTabHelperDelegate() = default;

void EnterpriseProxyTabHelperDelegate::SignIn(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // This case should not happen for normal tab execution, but can be reached in
  // case of shutdown races. Defensively return instead of CHECK to avoid
  // crashing.
  if (!profile) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  SigninBridge* signin_bridge = SigninBridgeFactory::GetForProfile(profile);
  if (!identity_manager || !signin_bridge) {
    return;
  }

  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  GURL destination_url = web_contents->GetVisibleURL();
  if (primary_account_id.empty()) {
    signin_bridge->OpenAccountPickerBottomSheetForWebSignin(
        web_contents, destination_url,
        /*account_id=*/std::nullopt);
  } else {
    TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
    if (tab) {
      signin_bridge->StartUpdateCredentialsFlow(tab, destination_url,
                                                primary_account_id);
    }
  }
#else
  // TODO(crbug.com/532559920): Implement desktop re-auth and login flow for
  // enterprise proxy.
#endif
}

}  // namespace enterprise_net
