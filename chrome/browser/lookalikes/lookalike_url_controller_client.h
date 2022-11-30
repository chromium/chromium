// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_CONTROLLER_CLIENT_H_

#include <memory>

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// Class for handling commands from lookalike URL interstitial pages.
class LookalikeUrlControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  static std::unique_ptr<security_interstitials::MetricsHelper>
  GetMetricsHelper(const GURL& url);

  LookalikeUrlControllerClient(
      content::WebContents* web_contents,
      const GURL& request_url,
      const GURL& safe_url);

  LookalikeUrlControllerClient(const LookalikeUrlControllerClient&) = delete;
  LookalikeUrlControllerClient& operator=(const LookalikeUrlControllerClient&) =
      delete;

  ~LookalikeUrlControllerClient() override;

  // security_interstitials::ControllerClient overrides.
  void GoBack() override;
  void Proceed() override;

 private:
  const GURL request_url_;
  const GURL safe_url_;
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_CONTROLLER_CLIENT_H_
