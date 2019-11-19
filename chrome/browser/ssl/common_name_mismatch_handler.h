// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_COMMON_NAME_MISMATCH_HANDLER_H_
#define CHROME_BROWSER_SSL_COMMON_NAME_MISMATCH_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// This class handles errors due to common name mismatches
// (|ERR_CERT_COMMON_NAME_INVALID|) and helps remediate them by suggesting
// alternative URLs that may be what the user intended to load.
class CommonNameMismatchHandler {
 public:
  enum SuggestedUrlCheckResult {
    // The request succeeds with good response code i.e. URL exists and its
    // certificate is valid.
    SUGGESTED_URL_AVAILABLE,
    // Suggested URL is either not available or has a bad certificate.
    SUGGESTED_URL_NOT_AVAILABLE
  };

  enum TestingState {
    NOT_TESTING,
    // Disables the actual request to the |suggested_url|.
    IGNORE_REQUESTS_FOR_TESTING
  };

  typedef base::Callback<void(SuggestedUrlCheckResult result,
                              const GURL& suggested_url)>
      CheckUrlCallback;

  CommonNameMismatchHandler(
      const GURL& request_url,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~CommonNameMismatchHandler();

  // Performs a network request to suggested URL. After completion, runs the
  // |callback|.
  void CheckSuggestedUrl(const GURL& url, const CheckUrlCallback& callback);

  // Determines if, for |request_url| serving a certificate that is valid for
  // the domain names |dns_names|, there is a name that the certificate is
  // valid for that closely matches the original name in |request_url|. If
  // so, returns true, and sets |*suggested_url| to a URL that is unlikely
  // to cause an ERR_CERT_COMMON_NAME_INVALID error.
  static bool GetSuggestedUrl(const GURL& request_url,
                              const std::vector<std::string>& dns_names,
                              GURL* suggested_url);

  // Used in tests, to disable the request to |suggested_url|.
  // If |testing_state| is IGNORE_REQUESTS_FOR_TESTING, then the
  // callback won't get called.
  static void set_state_for_testing(TestingState testing_state) {
    testing_state_ = testing_state;
  }

  // Cancels the request to |suggested_url|
  void Cancel();

 private:
  void OnSimpleLoaderHandler(const GURL& final_url,
                             const network::mojom::URLResponseHead* head);
  void OnSimpleLoaderRedirect(
      const net::RedirectInfo& redirect_info,
      const network::mojom::URLResponseHead& response_head,
      std::vector<std::string>* to_be_removed_headers);
  void OnSimpleLoaderResponseStarted(
      const GURL& final_url,
      const network::mojom::URLResponseHead& response_head);
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Returns true if the check is currently running.
  bool IsCheckingSuggestedUrl() const;

  static TestingState testing_state_;
  const GURL request_url_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  GURL check_url_;
  CheckUrlCallback check_url_callback_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CommonNameMismatchHandler);
};

#endif  // CHROME_BROWSER_SSL_COMMON_NAME_MISMATCH_HANDLER_H_
