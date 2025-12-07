// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_SAFE_BROWSING_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_SAFE_BROWSING_DELEGATE_H_

#include "extensions/browser/safe_browsing_delegate.h"

namespace extensions {

// Provides access to telemetry and safe browsing services for extensions.
// Requires full safe browsing to be enabled.
class ChromeSafeBrowsingDelegate : public SafeBrowsingDelegate {
 public:
  ChromeSafeBrowsingDelegate();
  ChromeSafeBrowsingDelegate(const ChromeSafeBrowsingDelegate&) = delete;
  ChromeSafeBrowsingDelegate& operator=(const ChromeSafeBrowsingDelegate&) =
      delete;
  ~ChromeSafeBrowsingDelegate() override;

  // SafeBrowsingDelegate:
  bool IsExtensionTelemetryServiceEnabled(
      content::BrowserContext* context) const override;
  void NotifyExtensionApiTabExecuteScript(
      content::BrowserContext* context,
      const ExtensionId& extension_id,
      const std::string& code) const override;
  void NotifyExtensionApiDeclarativeNetRequest(
      content::BrowserContext* context,
      const ExtensionId& extension_id,
      const std::vector<api::declarative_net_request::Rule>& rules)
      const override;
  void NotifyExtensionDeclarativeNetRequestRedirectAction(
      content::BrowserContext* context,
      const ExtensionId& extension_id,
      const GURL& request_url,
      const GURL& redirect_url) const override;
  void CreatePasswordReuseDetectionManager(
      content::WebContents* web_contents) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_SAFE_BROWSING_DELEGATE_H_
