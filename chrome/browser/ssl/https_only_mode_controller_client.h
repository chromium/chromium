// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_CONTROLLER_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"

namespace content {
class WebContents;
}  // namespace content

namespace security_interstitials {
class MetricsHelper;
}  // namespace security_interstitials

class HttpsOnlyModeControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  static std::unique_ptr<security_interstitials::MetricsHelper>
  GetMetricsHelper(const GURL& url);

  HttpsOnlyModeControllerClient(content::WebContents* web_contents,
                                const GURL& request_url);
  HttpsOnlyModeControllerClient(const HttpsOnlyModeControllerClient&) = delete;
  HttpsOnlyModeControllerClient& operator=(
      const HttpsOnlyModeControllerClient&) = delete;
  ~HttpsOnlyModeControllerClient() override;

  // security_interstitials::ControllerClient:
  void GoBack() override;
  void Proceed() override;

 private:
  raw_ptr<content::WebContents> web_contents_;
  const GURL request_url_;
};

#endif  // CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_CONTROLLER_CLIENT_H_
