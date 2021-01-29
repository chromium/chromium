// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_IMPL_H_

#include "build/buildflag.h"
#include "chrome/browser/signin/header_modification_delegate.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"

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
  bool ShouldInterceptNavigation(content::WebContents* contents) override;
  void ProcessRequest(ChromeRequestAdapter* request_adapter,
                      const GURL& redirect_url) override;
  void ProcessResponse(ResponseAdapter* response_adapter,
                       const GURL& redirect_url) override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Returns true if the request comes from a web view and should be ignored
  // (i.e. not intercepted).
  // Returns false if the request does not come from a web view.
  // Requests coming from most guest web views are ignored. In particular the
  // requests coming from the InlineLoginUI are not intercepted (see
  // http://crbug.com/428396). Requests coming from the chrome identity
  // extension consent flow are not ignored.
  static bool ShouldIgnoreGuestWebViewRequest(content::WebContents* contents);
#endif

 private:
  Profile* profile_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  DISALLOW_COPY_AND_ASSIGN(HeaderModificationDelegateImpl);
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_IMPL_H_
