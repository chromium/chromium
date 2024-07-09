// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_CONTROLLER_CLIENT_H_

#include <string>

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "url/gurl.h"

class PrefService;

namespace content {
class WebContents;
}  // namespace content

// Class for handling commands from the supervised user verification
// interstitial page.
class SupervisedUserVerificationControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  SupervisedUserVerificationControllerClient(content::WebContents* web_contents,
                                             PrefService* prefs,
                                             const std::string& app_locale,
                                             const GURL& default_safe_page,
                                             const GURL& request_url);

  SupervisedUserVerificationControllerClient(
      const SupervisedUserVerificationControllerClient&) = delete;
  SupervisedUserVerificationControllerClient& operator=(
      const SupervisedUserVerificationControllerClient&) = delete;

  ~SupervisedUserVerificationControllerClient() override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_CONTROLLER_CLIENT_H_
