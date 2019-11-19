// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SECURITY_STATE_TAB_HELPER_H_
#define CHROME_BROWSER_SSL_SECURITY_STATE_TAB_HELPER_H_

#include <memory>

#include "base/optional.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/common/security/security_style.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// Tab helper provides the page's security status. Also logs console warnings
// for private data on insecure pages.
class SecurityStateTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SecurityStateTabHelper> {
 public:
  ~SecurityStateTabHelper() override;

  // See security_state::GetSecurityLevel.
  security_state::SecurityLevel GetSecurityLevel();
  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState();

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidChangeVisibleSecurityState() override;

 private:
  explicit SecurityStateTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SecurityStateTabHelper>;

  bool UsedPolicyInstalledCertificate() const;
  security_state::MaliciousContentStatus GetMaliciousContentStatus() const;

  // Caches the legacy TLS warning suppression status for the duration of a page
  // load (bound to a specific navigation ID) to ensure that we show consistent
  // security UI (e.g., security indicator and page info). This is because the
  // status depends on external state (a component loading from disk), which can
  // cause inconsistent state across a page load if it isn't cached.
  base::Optional<std::pair<int /* navigation entry ID */,
                           bool /* should_suppress_legacy_tls_warning */>>
      cached_should_suppress_legacy_tls_warning_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelper);
};

#endif  // CHROME_BROWSER_SSL_SECURITY_STATE_TAB_HELPER_H_
