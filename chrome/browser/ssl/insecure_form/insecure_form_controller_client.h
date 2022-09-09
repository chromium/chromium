// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_INSECURE_FORM_INSECURE_FORM_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_SSL_INSECURE_FORM_INSECURE_FORM_CONTROLLER_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "components/security_interstitials/content/content_metrics_helper.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"

namespace content {
class WebContents;
}  // namespace content

class InsecureFormControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  static std::unique_ptr<security_interstitials::MetricsHelper>
  GetMetricsHelper(const GURL& url);

  InsecureFormControllerClient(content::WebContents* web_contents,
                               const GURL& form_target_url);
  InsecureFormControllerClient(const InsecureFormControllerClient&) = delete;
  InsecureFormControllerClient& operator=(const InsecureFormControllerClient&) =
      delete;
  ~InsecureFormControllerClient() override;

  // security_interstitials::ControllerClient:
  void GoBack() override;
  void Proceed() override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_SSL_INSECURE_FORM_INSECURE_FORM_CONTROLLER_CLIENT_H_
