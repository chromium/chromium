// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SECURITY_STATE_TAB_HELPER_H_
#define CHROME_BROWSER_SSL_SECURITY_STATE_TAB_HELPER_H_

#include <memory>

#include "base/functional/callback.h"
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
  SecurityStateTabHelper(const SecurityStateTabHelper&) = delete;
  SecurityStateTabHelper& operator=(const SecurityStateTabHelper&) = delete;

  ~SecurityStateTabHelper() override;

  // See security_state::GetSecurityLevel.
  security_state::SecurityLevel GetSecurityLevel();
  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState();

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;

  // Used by tests to specify a callback to be called when
  // GetVisibleSecurityState() is called.
  void set_get_security_level_callback_for_tests_(base::OnceClosure closure) {
    get_security_level_callback_for_tests_ = std::move(closure);
  }

 private:
  explicit SecurityStateTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SecurityStateTabHelper>;

  bool UsedPolicyInstalledCertificate() const;
  security_state::MaliciousContentStatus GetMaliciousContentStatus() const;

  base::OnceClosure get_security_level_callback_for_tests_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SSL_SECURITY_STATE_TAB_HELPER_H_
