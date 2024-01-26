// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_URL_FETCHER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_URL_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace net {
struct RedirectInfo;
}

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
}  // namespace network

namespace media_router {

// Used to make a single HTTP request with |url| to fetch a response
// from a DIAL device.  If successful, |success_cb| is invoked with the result;
// otherwise, |error_cb| is invoked with an error reason.
// This class is not sequence safe.
class DialURLFetcher {
 public:
  using SuccessCallback =
      base::OnceCallback<void(const std::string& app_info_xml)>;
  // |http_response_code| is set when one was received from the DIAL device.
  // It may be in the 200s if the error was with the content of the response,
  // e.g. if it was unexpectedly empty.
  using ErrorCallback =
      base::OnceCallback<void(const std::string& error_message,
                              std::optional<int> http_response_code)>;

  // |success_cb|: Invoked when HTTP request to |url| succeeds.
  // |error_cb|: Invoked when HTTP request to |url| fails.
  DialURLFetcher(SuccessCallback success_cb, ErrorCallback error_cb);

  DialURLFetcher(const DialURLFetcher&) = delete;
  DialURLFetcher& operator=(const DialURLFetcher&) = delete;

  virtual ~DialURLFetcher();

  // Starts a HTTP GET request.
  void Get(const GURL& url, bool set_origin_header = true);

  // Starts a HTTP DELETE request.
  void Delete(const GURL& url);

  // Starts a HTTP POST request.
  void Post(const GURL& url, const std::optional<std::string>& post_data);

  // Returns the response header of an HTTP request. The response header is
  // owned by underlying |loader_| object and is reset per HTTP request. Returns
  // nullptr if this function is called before |loader_| has informed the caller
  // of completion.
  const network::mojom::URLResponseHead* GetResponseHead() const;

  // If a non-nullptr |request| is passed, a copy of the resource request will
  // be stored in it when the request is started.  |request| must outlive the
  // call to Get(), Delete() or Post().
  void SetSavedRequestForTest(network::ResourceRequest* request) {
    saved_request_for_test_ = request;
  }

 private:
  friend class TestDialURLFetcher;

  // Starts the fetch with |method|. |ProcessResponse| will be invoked on
  // completion. |ReportRedirectError| will be invoked when a redirect occurrs.
  // |method|: the request method, e.g. GET, POST, etc.
  // |post_data|: optional request body (may be empty).
  // |max_retries|: the maximum number of times to retry the request, not
  // counting the initial request.
  // |set_origin_header|: whether to set an Origin: header on the request.
  virtual void Start(const GURL& url,
                     const std::string& method,
                     const std::optional<std::string>& post_data,
                     int max_retries,
                     bool set_origin_header);

  // Starts the download on |loader_|.
  virtual void StartDownload();

  // Processes the response and invokes the success or error callback.
  void ProcessResponse(std::unique_ptr<std::string> response);

  // Invokes the error callback due to redirect, and aborts the request.
  void ReportRedirectError(const GURL& url_before_redirect,
                           const net::RedirectInfo& redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           std::vector<std::string>* to_be_removed_headers);

  // Runs |error_cb_| with |message| and clears it.
  void ReportError(const std::string& message);

  // Returns the HTTP code in the response header, if exists.
  virtual std::optional<int> GetHttpResponseCode() const;

  SuccessCallback success_cb_;
  ErrorCallback error_cb_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // The HTTP method that was started on the fetcher (e.g., "GET").
  std::string method_;
  raw_ptr<network::ResourceRequest> saved_request_for_test_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_URL_FETCHER_H_
