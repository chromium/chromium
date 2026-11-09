// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_PROXY_TAB_HELPER_PLATFORM_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_PROXY_TAB_HELPER_PLATFORM_DELEGATE_ANDROID_H_

#include "chrome/browser/enterprise/net/enterprise_proxy_tab_helper_delegate.h"

namespace enterprise_net {

// Android implementation of
// `EnterpriseProxyTabHelperDelegate::PlatformDelegate` backed by
// `SigninBridge`.
class EnterpriseProxyTabHelperPlatformDelegateAndroid
    : public EnterpriseProxyTabHelperDelegate::PlatformDelegate {
 public:
  EnterpriseProxyTabHelperPlatformDelegateAndroid();
  EnterpriseProxyTabHelperPlatformDelegateAndroid(
      const EnterpriseProxyTabHelperPlatformDelegateAndroid&) = delete;
  EnterpriseProxyTabHelperPlatformDelegateAndroid& operator=(
      const EnterpriseProxyTabHelperPlatformDelegateAndroid&) = delete;
  ~EnterpriseProxyTabHelperPlatformDelegateAndroid() override;

  // EnterpriseProxyTabHelperDelegate::PlatformDelegate:
  void StartSignInFlow(Profile* profile,
                       content::WebContents* web_contents,
                       const GURL& destination_url) override;
  void StartReauthFlow(Profile* profile,
                       content::WebContents* web_contents,
                       const GURL& destination_url,
                       const CoreAccountId& account_id) override;
};

}  // namespace enterprise_net

#endif  // CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_PROXY_TAB_HELPER_PLATFORM_DELEGATE_ANDROID_H_
