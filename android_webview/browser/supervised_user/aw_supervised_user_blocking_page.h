// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_BLOCKING_PAGE_H_
#define ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_BLOCKING_PAGE_H_

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {
class AwSupervisedUserBlockingPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  static std::unique_ptr<security_interstitials::SecurityInterstitialPage>
  CreateBlockingPage(content::WebContents* web_contents, const GURL& url);
  AwSupervisedUserBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);
  AwSupervisedUserBlockingPage(const AwSupervisedUserBlockingPage&) = delete;
  AwSupervisedUserBlockingPage& operator=(const AwSupervisedUserBlockingPage&) =
      delete;

  ~AwSupervisedUserBlockingPage() override;

 protected:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;
  void OnInterstitialClosing() override;
  int GetHTMLTemplateId() override;
};
}  // namespace android_webview
#endif  // ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_BLOCKING_PAGE_H_
