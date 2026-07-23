// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_CHROME_WEBSTORE_PRIVATE_API_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_CHROME_WEBSTORE_PRIVATE_API_DELEGATE_H_

#include "extensions/browser/api/webstore_private/webstore_private_api_delegate.h"

namespace extensions {

// Chrome implementation of WebstorePrivateAPIDelegate.
class ChromeWebstorePrivateAPIDelegate : public WebstorePrivateAPIDelegate {
 public:
  ChromeWebstorePrivateAPIDelegate();
  ~ChromeWebstorePrivateAPIDelegate() override;

  // WebstorePrivateAPIDelegate:
  std::vector<KeyedServiceBaseFactory*> GetWebStoreAPIFactoryDependencies()
      override;
  ExtensionAllowlist* GetExtensionAllowlist(
      content::BrowserContext* context) override;
  signin::IdentityManager* GetIdentityManager(
      content::BrowserContext* context) override;
  void ShowExtensionInstallBlockedDialog(
      content::WebContents* web_contents,
      const Extension* extension,
      const std::u16string& custom_error_message,
      const gfx::ImageSkia& icon,
      base::OnceClosure done_callback) override;
  void ShowExtensionInstallFrictionDialog(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> callback) override;
#if BUILDFLAG(IS_ANDROID)
  void ShowExtensionInstallAskParentDialog(
      content::WebContents* web_contents,
      base::OnceClosure cancel_callback,
      base::OnceClosure approve_callback) override;
#endif  // BUILDFLAG(IS_ANDROID)
  void ReportFrictionAcceptedEvent(content::BrowserContext* context) override;
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  bool IsSafeBrowsingEnabledAndReady(content::BrowserContext* context) override;
  safe_browsing::SafeBrowsingNavigationObserverManager*
  GetSafeBrowsingNavigationObserverManager(
      content::BrowserContext* context) override;
#endif
  std::unique_ptr<enterprise_promotion::PromotionEligibilityChecker>
  CreatePromotionEligibilityChecker(content::BrowserContext* context,
                                    bool dismissed_banner_pref,
                                    bool feature_enabled) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_CHROME_WEBSTORE_PRIVATE_API_DELEGATE_H_
