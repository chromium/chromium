// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_H_
#define CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_H_

class GURL;

namespace content {
class WebContents;
}

namespace signin {

class ChromeRequestAdapter;
class ResponseAdapter;

class HeaderModificationDelegate {
 public:
  HeaderModificationDelegate() = default;

  HeaderModificationDelegate(const HeaderModificationDelegate&) = delete;
  HeaderModificationDelegate& operator=(const HeaderModificationDelegate&) =
      delete;

  virtual ~HeaderModificationDelegate() = default;

  virtual bool ShouldInterceptNavigation(content::WebContents* contents) = 0;
  virtual void ProcessRequest(ChromeRequestAdapter* request_adapter,
                              const GURL& redirect_url) = 0;
  virtual void ProcessResponse(ResponseAdapter* response_adapter,
                               const GURL& redirect_url) = 0;
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_HEADER_MODIFICATION_DELEGATE_H_
