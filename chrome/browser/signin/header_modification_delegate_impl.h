// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_IMPL_H_

#include "chrome/browser/signin/header_modification_delegate.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/browser_thread.h"

class Profile;

namespace signin {

// This class wraps the FixAccountConsistencyRequestHeader and
// ProcessAccountConsistencyResponseHeaders in the HeaderModificationDelegate
// interface.
class HeaderModificationDelegateImpl : public HeaderModificationDelegate {
 public:
  explicit HeaderModificationDelegateImpl(Profile* profile);
  ~HeaderModificationDelegateImpl() override;

  // HeaderModificationDelegate
  bool ShouldInterceptNavigation(
      content::NavigationUIData* navigation_ui_data) override;
  void ProcessRequest(ChromeRequestAdapter* request_adapter,
                      const GURL& redirect_url) override;
  void ProcessResponse(ResponseAdapter* response_adapter,
                       const GURL& redirect_url) override;

 private:
  Profile* profile_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  DISALLOW_COPY_AND_ASSIGN(HeaderModificationDelegateImpl);
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_IMPL_H_
