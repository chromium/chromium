// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SECURITY_BLOCKING_PAGE_FACTORY_H_
#define CHROME_BROWSER_SSL_CHROME_SECURITY_BLOCKING_PAGE_FACTORY_H_

#include "build/build_config.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/blocked_interception_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#include "components/security_interstitials/content/https_only_mode_blocking_page.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/security_blocking_page_factory.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_blocking_page_base.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"

// //chrome's implementation of the SecurityBlockingPageFactory interface.
class ChromeSecurityBlockingPageFactory : public SecurityBlockingPageFactory {
 public:
  ChromeSecurityBlockingPageFactory() = default;
  ~ChromeSecurityBlockingPageFactory() override = default;
  ChromeSecurityBlockingPageFactory(const ChromeSecurityBlockingPageFactory&) =
      delete;
  ChromeSecurityBlockingPageFactory& operator=(
      const ChromeSecurityBlockingPageFactory&) = delete;

  // SecurityBlockingPageFactory:
  std::unique_ptr<SSLBlockingPage> CreateSSLPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      const GURL& support_url) override;
  std::unique_ptr<CaptivePortalBlockingPage> CreateCaptivePortalBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      const GURL& login_url,
      const net::SSLInfo& ssl_info,
      int cert_error) override;
  std::unique_ptr<BadClockBlockingPage> CreateBadClockBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      const base::Time& time_triggered,
      ssl_errors::ClockState clock_state) override;
  std::unique_ptr<MITMSoftwareBlockingPage> CreateMITMSoftwareBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      const net::SSLInfo& ssl_info,
      const std::string& mitm_software_name) override;
  std::unique_ptr<BlockedInterceptionBlockingPage>
  CreateBlockedInterceptionBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      const net::SSLInfo& ssl_info) override;
  std::unique_ptr<security_interstitials::InsecureFormBlockingPage>
  CreateInsecureFormBlockingPage(content::WebContents* web_contents,
                                 const GURL& request_url) override;
  std::unique_ptr<security_interstitials::HttpsOnlyModeBlockingPage>
  CreateHttpsOnlyModeBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      security_interstitials::https_only_mode::HttpInterstitialState
          interstitial_state) override;

  // Overrides the calculation of whether the app is enterprise-managed for
  // tests.
  static void SetEnterpriseManagedForTesting(bool enterprise_managed);

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Opens a login tab if the profile's active window doesn't have one already.
  static void OpenLoginTabForWebContents(content::WebContents* web_contents,
                                         bool focus);
#endif
};

#endif  // CHROME_BROWSER_SSL_CHROME_SECURITY_BLOCKING_PAGE_FACTORY_H_
