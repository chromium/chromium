// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/enterprise_proxy_tab_helper_platform_delegate_android.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace enterprise_net {

// static
std::unique_ptr<EnterpriseProxyTabHelperDelegate::PlatformDelegate>
EnterpriseProxyTabHelperDelegate::PlatformDelegate::Create() {
  return std::make_unique<EnterpriseProxyTabHelperPlatformDelegateAndroid>();
}

EnterpriseProxyTabHelperPlatformDelegateAndroid::
    EnterpriseProxyTabHelperPlatformDelegateAndroid() = default;

EnterpriseProxyTabHelperPlatformDelegateAndroid::
    ~EnterpriseProxyTabHelperPlatformDelegateAndroid() = default;

void EnterpriseProxyTabHelperPlatformDelegateAndroid::StartSignInFlow(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& destination_url) {
  if (!profile || !web_contents) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  SigninBridge* signin_bridge = SigninBridgeFactory::GetForProfile(profile);
  if (!identity_manager || !signin_bridge) {
    return;
  }

  std::vector<CoreAccountInfo> accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  if (accounts.empty()) {
    if (TabAndroid* tab = TabAndroid::FromWebContents(web_contents)) {
      signin_bridge->StartAddAccountFlow(tab, /*prefilled_email=*/std::string(),
                                         destination_url,
                                         /*extension_name=*/std::string());
    }
    return;
  }

  signin_bridge->OpenAccountPickerBottomSheetForWebSignin(
      web_contents, destination_url, accounts.front().account_id);
}

void EnterpriseProxyTabHelperPlatformDelegateAndroid::StartReauthFlow(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& destination_url,
    const CoreAccountId& account_id) {
  if (!profile || !web_contents) {
    return;
  }

  SigninBridge* signin_bridge = SigninBridgeFactory::GetForProfile(profile);
  if (!signin_bridge) {
    return;
  }

  if (TabAndroid* tab = TabAndroid::FromWebContents(web_contents)) {
    signin_bridge->StartUpdateCredentialsFlow(tab, destination_url,
                                              /*account_to_reauth=*/account_id,
                                              /*target_account_id=*/account_id);
  }
}

}  // namespace enterprise_net
