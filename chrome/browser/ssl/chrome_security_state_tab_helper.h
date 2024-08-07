// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_TAB_HELPER_H_
#define CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_TAB_HELPER_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/common/security/security_style.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// This class extends the SecurityStateTabHelper with functionality that is
// available in Chrome. In addition to using the `VisibleSecurityState` of the
// base class, it considers:
//  - the SafeBrowsingService to identify potentially malicious sites,
//  - the PolicyCertService on ChromeOs to check for cached content from an
//    untrusted source,
//  - the SafeTipInfo that is provided for the current page,
//  - the profile prefs to change how mixed forms are treated, and
//  - the HttpsOnlyModeTabHelper providing HTTPS-Only Mode data.
//
// Furthermove, it logs console warnings for private data on insecure pages.
class ChromeSecurityStateTabHelper : public SecurityStateTabHelper,
                                     public content::WebContentsObserver {
 public:
  // Creates a new `ChromeSecurityStateTabHelper` that can be accessed by using
  // the `SecurityStateTabHelper::From` method. It does not replace any existing
  // `SecurityStateTabHelper` but CHECKs that none was created. Every component
  // used with chrome/ should have the elevated checks provided by this clss.
  static void CreateForWebContents(content::WebContents* contents);

  ChromeSecurityStateTabHelper(const ChromeSecurityStateTabHelper&) = delete;
  ChromeSecurityStateTabHelper& operator=(const ChromeSecurityStateTabHelper&) =
      delete;

  ~ChromeSecurityStateTabHelper() override;

  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;

 private:
  explicit ChromeSecurityStateTabHelper(content::WebContents* web_contents);

  security_state::MaliciousContentStatus GetMaliciousContentStatus() const;
  bool UsedPolicyInstalledCertificate() const override;
};

#endif  // CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_TAB_HELPER_H_
