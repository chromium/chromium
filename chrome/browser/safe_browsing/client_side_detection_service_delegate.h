// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_

#include "components/safe_browsing/content/browser/client_side_detection_service.h"

class Profile;

namespace safe_browsing {

// Delegate class which implements chrome specific bits for configuring
// the ClientSideDetectionService class.
class ClientSideDetectionServiceDelegate
    : public ClientSideDetectionService::Delegate {
 public:
  explicit ClientSideDetectionServiceDelegate(Profile* profile);
  ~ClientSideDetectionServiceDelegate() override;

  // ClientSideDetectionService::Delegate implementation.
  PrefService* GetPrefs() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSafeBrowsingURLLoaderFactory() override;
  ChromeUserPopulation GetUserPopulation() override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionServiceDelegate);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_DELEGATE_H_
