// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_WEB_REQUEST_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_WEB_REQUEST_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "chrome/services/wilco_dtc_supportd/public/mojom/wilco_dtc_supportd.mojom.h"
#include "url/gurl.h"

namespace network {

struct ResourceRequest;
class SharedURLLoaderFactory;
class SimpleURLLoader;

}  // namespace network

namespace chromeos {

// Max number of supported pending web requests.
extern const int kWilcoDtcSupportdWebRequestQueueMaxSize;

// Max size of web response body in bytes.
extern const int kWilcoDtcSupportdWebResponseMaxSizeInBytes;

// This class manages and performs web requests initiated by
// wilco_dtc_supportd_processor. This service performs only one request at a
// time and queues additional incoming requests. It can handle the limited
// number of queued web requests. If the web request queue overflows, new web
// requests fail with kNetworkError.
class WilcoDtcSupportdWebRequestService final {
 public:
  using PerformWebRequestCallback = base::OnceCallback<void(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus,
      int,
      mojo::ScopedHandle)>;

  explicit WilcoDtcSupportdWebRequestService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~WilcoDtcSupportdWebRequestService();

  // Performs web request. The response is returned by |callback| which is
  // guaranteed to be called. The requests, that were not complete in lifetime
  // of the service, will be canceled and the |callback| will be executed in
  // the destructor and fail with kNetworkError.
  void PerformRequest(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod
          http_method,
      GURL url,
      std::vector<base::StringPiece> headers,
      std::string request_body,
      PerformWebRequestCallback callback);

  int request_queue_size_for_testing() { return request_queue_.size(); }

 private:
  struct WebRequest {
    WebRequest();
    ~WebRequest();

    std::unique_ptr<network::ResourceRequest> request;
    std::string request_body;
    PerformWebRequestCallback callback;
  };
  // Starts the next web request from the web request queue if there is no
  // |active_request_| and |request_queue_| is not empty.
  void MaybeStartNextRequest();

  // On the |active_request_| completed. Runs the |active_request_.callback|.
  // Starts the next web request if there is any in the |request_queue_|.
  void OnRequestComplete(std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Should be reset for every web request.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::queue<std::unique_ptr<WebRequest>> request_queue_;
  std::unique_ptr<WebRequest> active_request_;

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdWebRequestService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_WEB_REQUEST_SERVICE_H_
