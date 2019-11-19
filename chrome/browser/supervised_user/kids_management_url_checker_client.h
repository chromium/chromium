// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_MANAGEMENT_URL_CHECKER_CLIENT_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_MANAGEMENT_URL_CHECKER_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client.h"
#include "components/safe_search_api/url_checker_client.h"

class GURL;

// TODO(crbug.com/988428): Change comments to use KidsChromeManagement instead
// of KidsManagement when migration is complete.

// This class uses the KidsManagement ClassifyUrl to check the classification
// of the content on a given URL and returns the result asynchronously
// via a callback.
class KidsManagementURLCheckerClient
    : public safe_search_api::URLCheckerClient {
 public:
  // |country| should be a two-letter country code (ISO 3166-1 alpha-2), e.g.,
  // "us".
  explicit KidsManagementURLCheckerClient(const std::string& country);
  ~KidsManagementURLCheckerClient() override;

  // Checks whether an |url| is restricted according to KidsManagement
  // ClassifyUrl RPC.
  //
  // On failure, the |callback| function is called with |url| as the first
  // parameter, and UNKNOWN as the second.
  void CheckURL(const GURL& url, ClientCheckCallback callback) override;

 private:
  void ConvertResponseCallback(
      const GURL& url,
      ClientCheckCallback client_callback,
      std::unique_ptr<google::protobuf::MessageLite> response_proto,
      KidsChromeManagementClient::ErrorCode error_code);

  const std::string country_;

  DISALLOW_COPY_AND_ASSIGN(KidsManagementURLCheckerClient);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_MANAGEMENT_URL_CHECKER_CLIENT_H_
