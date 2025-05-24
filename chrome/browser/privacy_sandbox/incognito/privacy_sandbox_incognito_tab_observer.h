// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_TAB_OBSERVER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_TAB_OBSERVER_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace privacy_sandbox {

class PrivacySandboxIncognitoTabObserver : public content::WebContentsObserver {
 public:
  explicit PrivacySandboxIncognitoTabObserver(
      content::WebContents* web_contents);
  ~PrivacySandboxIncognitoTabObserver() override;

 protected:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  bool IsNewTabPage(const GURL& url);
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_TAB_OBSERVER_H_
