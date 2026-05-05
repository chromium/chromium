// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_BLOCKING_PAGE_H_
#define ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_BLOCKING_PAGE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace android_webview {

class AwContentRestrictionManagerClient;

// Blocking page implementation for requests blocked by content restriction.
class AwContentRestrictionBlockingPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  static std::unique_ptr<security_interstitials::SecurityInterstitialPage>
  CreateBlockingPage(
      content::WebContents* web_contents,
      const GURL& url,
      AwContentRestrictionManagerClient* content_restriction_manager_client);

  AwContentRestrictionBlockingPage(const AwContentRestrictionBlockingPage&) =
      delete;
  AwContentRestrictionBlockingPage& operator=(
      const AwContentRestrictionBlockingPage&) = delete;
  ~AwContentRestrictionBlockingPage() override;

 protected:
  // SecurityInterstitialPage:
  bool ShouldDisplayURL() const override;
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::DictValue& load_time_data) override;
  void OnInterstitialClosing() override;
  int GetHTMLTemplateId() override;

 private:
  AwContentRestrictionBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      AwContentRestrictionManagerClient* content_restriction_manager_client,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

  const raw_ptr<AwContentRestrictionManagerClient>
      content_restriction_manager_client_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_BLOCKING_PAGE_H_
