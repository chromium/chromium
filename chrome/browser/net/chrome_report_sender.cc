// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_report_sender.h"

#include "base/bind.h"
#include "net/url_request/report_sender.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// The value doesn't really matter, as we'll delete the loader once we get the
// response.
static const int kMaxSize = 1024;

using ErrorCallback =
    base::OnceCallback<void(int /* net_error */, int /* http_response_code */)>;

// Owns the SimpleURLLoader and will run the appropriate callback and delete
// the loader when the response arrives.
class SimpleURLLoaderOwner {
 public:
  SimpleURLLoaderOwner(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<network::SimpleURLLoader> loader,
      base::OnceClosure success_callback,
      ErrorCallback error_callback)
      : loader_(std::move(loader)),
        success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)) {
    // We don't care to read the response, and since it can come from untrusted
    // endpoints it's better to not buffer. So we'll match net::ReportSender by
    // closing the loader as soon as we start getting the response.
    loader_->SetOnResponseStartedCallback(base::BindOnce(
        &SimpleURLLoaderOwner::OnResponseStarted, base::Unretained(this)));
    loader_->DownloadToString(
        url_loader_factory.get(),
        base::BindOnce(&SimpleURLLoaderOwner::OnURLLoaderComplete,
                       base::Unretained(this)),
        kMaxSize);
  }

 private:
  ~SimpleURLLoaderOwner() = default;

  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head) {
    OnDone(&response_head, net::OK);
  }

  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body) {
    OnDone(loader_->ResponseInfo(), loader_->NetError());
  }

  void OnDone(const network::mojom::URLResponseHead* response_head,
              int net_error) {
    if (net_error == net::OK) {
      if (success_callback_)
        std::move(success_callback_).Run();
    } else if (error_callback_) {
      int response_code = 0;
      if (response_head && response_head->headers)
        response_code = response_head->headers->response_code();
      std::move(error_callback_).Run(loader_->NetError(), response_code);
    }
    delete this;
  }

  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::OnceClosure success_callback_;
  ErrorCallback error_callback_;
};

}  // namespace

void SendReport(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const GURL& report_uri,
    const std::string& content_type,
    const std::string& report,
    base::OnceClosure success_callback,
    ErrorCallback error_callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = report_uri;
  resource_request->method = "POST";
  resource_request->load_flags = net::ReportSender::kLoadFlags;

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  loader->AttachStringForUpload(report, content_type);

  new SimpleURLLoaderOwner(url_loader_factory, std::move(loader),
                           std::move(success_callback),
                           std::move(error_callback));
}
