// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_BLOCK_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_BLOCK_CONTROLLER_CLIENT_H_

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// Class for handling commands from Enterprise Block interstitial page.
class EnterpriseBlockControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  EnterpriseBlockControllerClient(content::WebContents* web_contents,
                                  const GURL& request_url);

  EnterpriseBlockControllerClient(const EnterpriseBlockControllerClient&) =
      delete;
  EnterpriseBlockControllerClient& operator=(
      const EnterpriseBlockControllerClient&) = delete;

  ~EnterpriseBlockControllerClient() override;

  // security_interstitials::ControllerClient overrides.
  void GoBack() override;

 private:
  const GURL request_url_;
};

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_BLOCK_CONTROLLER_CLIENT_H_
