// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_URL_FETCHER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_URL_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace net {
struct RedirectInfo;
}

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace media_router {

// Used to make a single HTTP GET request with |url| to fetch a response
// from a DIAL device.  If successful, |success_cb| is invoked with the result;
// otherwise, |error_cb| is invoked with an error reason.
// This class is not sequence safe.
class DialURLFetcher {
 public:
  using SuccessCallback = base::OnceCallback<void(const std::string&)>;
  using ErrorCallback = base::OnceCallback<void(int, const std::string&)>;

  // |success_cb|: Invoked when HTTP request to |url| succeeds
  //   |arg 0|: response text of the HTTP request
  // |error_cb|: Invoked when HTTP request to |url| fails
  //   |arg 0|: HTTP response code
  //   |arg 1|: error message
  DialURLFetcher(SuccessCallback success_cb, ErrorCallback error_cb);

  virtual ~DialURLFetcher();

  // Starts a HTTP GET request.
  void Get(const GURL& url);

  // Starts a HTTP DELETE request.
  void Delete(const GURL& url);

  // Starts a HTTP POST request.
  void Post(const GURL& url, const base::Optional<std::string>& post_data);

  // Returns the response header of an HTTP request. The response header is
  // owned by underlying |loader_| object and is reset per HTTP request. Returns
  // nullptr if this function is called before |loader_| has informed the caller
  // of completion.
  const network::mojom::URLResponseHead* GetResponseHead() const;

 private:
  friend class TestDialURLFetcher;

  // Starts the fetch with |method|. |ProcessResponse| will be invoked on
  // completion. |ReportRedirectError| will be invoked when a redirect occurrs.
  // |method|: the request method, e.g. GET, POST, etc.
  // |post_data|: optional request body (may be empty).
  // |max_retries|: the maximum number of times to retry the request, not
  // counting the initial request.
  virtual void Start(const GURL& url,
                     const std::string& method,
                     const base::Optional<std::string>& post_data,
                     int max_retries);

  // Starts the download on |loader_|.
  virtual void StartDownload();

  // Processes the response and invokes the success or error callback.
  void ProcessResponse(std::unique_ptr<std::string> response);

  // Invokes the error callback due to redirect, and aborts the request.
  void ReportRedirectError(const net::RedirectInfo& redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           std::vector<std::string>* to_be_removed_headers);

  // Runs |error_cb_| with |message| and clears it.
  void ReportError(int response_code, const std::string& message);

  SuccessCallback success_cb_;
  ErrorCallback error_cb_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // The HTTP method that was started on the fetcher (e.g., "GET").
  std::string method_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(DialURLFetcher);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_URL_FETCHER_H_
