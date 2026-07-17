// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_client.h"

#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_request_headers.h"
#include "net/ssl/ssl_info.h"

namespace enterprise_custom_headers {

namespace {

void PopulateNetLogEvents(
    const net::HttpRequestHeaders& headers_to_inject,
    const net::HttpRequestHeaders& final_headers,
    std::optional<base::DictValue>& extended_net_log_events) {
  base::DictValue injection_events;
  for (net::HttpRequestHeaders::Iterator it(headers_to_inject); it.GetNext();) {
    injection_events.Set(
        it.name(), base::DictValue()
                       .Set("value", it.value())
                       .Set("is_override", final_headers.HasHeader(it.name())));
  }

  if (!extended_net_log_events) {
    extended_net_log_events = base::DictValue();
  }
  extended_net_log_events->Set("http_header_injection_policy",
                               std::move(injection_events));
}

}  // namespace

// static
void HttpHeaderInjectionClient::Create(
    base::WeakPtr<HttpHeaderInjectionService> service,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_client) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new HttpHeaderInjectionClient(std::move(service),
                                                     std::move(target_client))),
      std::move(receiver));
}

HttpHeaderInjectionClient::HttpHeaderInjectionClient(
    base::WeakPtr<HttpHeaderInjectionService> service,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_client)
    : service_(std::move(service)) {
  if (target_client) {
    target_client_.Bind(std::move(target_client));
    target_client_.set_disconnect_handler(
        base::BindOnce(&HttpHeaderInjectionClient::OnTargetDisconnect,
                       base::Unretained(this)));
  }
}

HttpHeaderInjectionClient::~HttpHeaderInjectionClient() = default;

void HttpHeaderInjectionClient::OnBeforeSendHeaders(
    const GURL& request_url,
    const net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  if (target_client_.is_bound()) {
    target_client_->OnBeforeSendHeaders(
        request_url, headers,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(
                &HttpHeaderInjectionClient::OnTargetBeforeSendHeadersComplete,
                weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                request_url, headers),
            net::ERR_FAILED, std::nullopt, std::nullopt));
  } else {
    OnTargetBeforeSendHeadersComplete(std::move(callback), request_url, headers,
                                      net::OK, std::nullopt, std::nullopt);
  }
}

void HttpHeaderInjectionClient::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& remote_endpoint,
    const std::optional<net::SSLInfo>& ssl_info,
    OnHeadersReceivedCallback callback) {
  if (target_client_.is_bound()) {
    target_client_->OnHeadersReceived(
        headers, remote_endpoint, ssl_info,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            std::move(callback), net::ERR_FAILED, std::nullopt, std::nullopt));
  } else {
    std::move(callback).Run(net::OK, std::nullopt, std::nullopt);
  }
}

void HttpHeaderInjectionClient::OnTargetDisconnect() {
  target_client_.reset();
}

void HttpHeaderInjectionClient::OnTargetBeforeSendHeadersComplete(
    OnBeforeSendHeadersCallback callback,
    const GURL& request_url,
    const net::HttpRequestHeaders& original_headers,
    int32_t result,
    const std::optional<net::HttpRequestHeaders>& headers,
    std::optional<base::DictValue> extended_net_log_events) {
  if (result != net::OK) {
    std::move(callback).Run(result, std::nullopt, std::nullopt);
    return;
  }

  std::optional<net::HttpRequestHeaders> final_headers = headers;

  if (!service_) {
    std::move(callback).Run(net::OK, final_headers,
                            std::move(extended_net_log_events));
    return;
  }

  net::HttpRequestHeaders headers_to_inject =
      service_->GetHeadersForUrl(request_url);

  bool modified = !headers_to_inject.IsEmpty();
  if (base::ShouldRecordSubsampledMetric(0.001)) {
    base::UmaHistogramBoolean("Enterprise.HttpHeaderInjection.RequestModified",
                              modified);
  }

  if (modified) {
    if (!final_headers) {
      final_headers = original_headers;
    }

    PopulateNetLogEvents(headers_to_inject, *final_headers,
                         extended_net_log_events);

    final_headers->MergeFrom(headers_to_inject);
  }

  std::move(callback).Run(net::OK, final_headers,
                          std::move(extended_net_log_events));
}

}  // namespace enterprise_custom_headers
