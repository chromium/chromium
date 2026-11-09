// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_PROXY_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_PROXY_TAB_HELPER_DELEGATE_H_

#include <memory>

#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"
#include "google_apis/gaia/core_account_id.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace enterprise_net {

// Chrome browser-level implementation of EnterpriseProxyTabHelper::Delegate.
class EnterpriseProxyTabHelperDelegate
    : public EnterpriseProxyTabHelper::Delegate {
 public:
  // Platform-specific delegate for launching sign-in and credential-recovery UI
  // flows when a valid primary account refresh token is not already available.
  class PlatformDelegate {
   public:
    virtual ~PlatformDelegate() = default;

    // Creates the platform-specific delegate (currently implemented on
    // Android), or returns `nullptr` on platforms where proxy sign-in UI is not
    // yet supported.
    static std::unique_ptr<PlatformDelegate> Create();

    // Starts the sign-in flow when no primary account is signed in, redirecting
    // to `destination_url` upon completion.
    virtual void StartSignInFlow(Profile* profile,
                                 content::WebContents* web_contents,
                                 const GURL& destination_url) = 0;

    // Starts the credential update (re-authentication) flow for `account_id`,
    // redirecting to `destination_url` upon completion.
    virtual void StartReauthFlow(Profile* profile,
                                 content::WebContents* web_contents,
                                 const GURL& destination_url,
                                 const CoreAccountId& account_id) = 0;
  };

  EnterpriseProxyTabHelperDelegate();
  explicit EnterpriseProxyTabHelperDelegate(
      std::unique_ptr<PlatformDelegate> platform_delegate);
  EnterpriseProxyTabHelperDelegate(const EnterpriseProxyTabHelperDelegate&) =
      delete;
  EnterpriseProxyTabHelperDelegate& operator=(
      const EnterpriseProxyTabHelperDelegate&) = delete;
  ~EnterpriseProxyTabHelperDelegate() override;

  // EnterpriseProxyTabHelper::Delegate:
  void SignIn(content::WebContents* web_contents) override;

 private:
  std::unique_ptr<PlatformDelegate> platform_delegate_;
};

}  // namespace enterprise_net

#endif  // CHROME_BROWSER_ENTERPRISE_NET_ENTERPRISE_PROXY_TAB_HELPER_DELEGATE_H_
