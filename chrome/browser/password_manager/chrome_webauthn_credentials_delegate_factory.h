// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_FACTORY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

class ChromeWebAuthnCredentialsDelegateFactory
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          ChromeWebAuthnCredentialsDelegateFactory> {
 public:
  ChromeWebAuthnCredentialsDelegateFactory(
      const ChromeWebAuthnCredentialsDelegateFactory&) = delete;
  ChromeWebAuthnCredentialsDelegateFactory& operator=(
      const ChromeWebAuthnCredentialsDelegateFactory&) = delete;

  ~ChromeWebAuthnCredentialsDelegateFactory() override;

  // Returns the instance of ChromeWebAuthnCredentialsDelegateFactory for the
  // given |web_contents|, creating one if it doesn't already exist.
  static ChromeWebAuthnCredentialsDelegateFactory* GetFactory(
      content::WebContents* web_contents);

  // Returns the delegate for the given frame, creating one if possible.
  // Can return nullptr.
  ChromeWebAuthnCredentialsDelegate* GetDelegateForFrame(
      content::RenderFrameHost* frame_host);

 private:
  friend class content::WebContentsUserData<
      ChromeWebAuthnCredentialsDelegateFactory>;

  explicit ChromeWebAuthnCredentialsDelegateFactory(
      content::WebContents* web_contents);

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  base::flat_map<content::RenderFrameHost*,
                 std::unique_ptr<ChromeWebAuthnCredentialsDelegate>>
      delegate_map_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_FACTORY_H_
