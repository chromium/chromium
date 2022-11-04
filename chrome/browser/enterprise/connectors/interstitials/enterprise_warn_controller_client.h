// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_WARN_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_WARN_CONTROLLER_CLIENT_H_

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// Class for handling commands from Enterprise Warn interstitial page.
class EnterpriseWarnControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  EnterpriseWarnControllerClient(content::WebContents* web_contents,
                                 const GURL& request_url);

  EnterpriseWarnControllerClient(const EnterpriseWarnControllerClient&) =
      delete;
  EnterpriseWarnControllerClient& operator=(
      const EnterpriseWarnControllerClient&) = delete;

  ~EnterpriseWarnControllerClient() override;

  // security_interstitials::ControllerClient overrides.
  void GoBack() override;
  void Proceed() override;

 private:
  const GURL request_url_;
};

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_WARN_CONTROLLER_CLIENT_H_
