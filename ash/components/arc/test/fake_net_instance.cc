// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_net_instance.h"

namespace arc {

FakeNetInstance::FakeNetInstance() {}

FakeNetInstance::~FakeNetInstance() = default;

void FakeNetInstance::Init(::mojo::PendingRemote<mojom::NetHost> host_remote,
                           InitCallback callback) {}

void FakeNetInstance::ScanCompleted() {}

void FakeNetInstance::WifiEnabledStateChanged(bool is_enabled) {}

void FakeNetInstance::DisconnectAndroidVpn() {}

void FakeNetInstance::ConfigureAndroidVpn() {}

void FakeNetInstance::ActiveNetworksChanged(
    std::vector<mojom::NetworkConfigurationPtr> network) {}

void FakeNetInstance::DnsResolutionTest(const std::string& transport_name,
                                        const std::string& host_name,
                                        DnsResolutionTestCallback callback) {
  mojom::ArcDnsResolutionTestResultPtr result_ptr =
      mojom::ArcDnsResolutionTestResult::New();
  result_ptr->is_successful = dns_resolution_test_result_.is_successful;
  result_ptr->response_code = dns_resolution_test_result_.response_code;
  result_ptr->duration_ms = dns_resolution_test_result_.duration_ms;
  std::move(callback).Run(std::move(result_ptr));
}

void FakeNetInstance::HttpTest(const std::string& transport_name,
                               const ::GURL& url,
                               HttpTestCallback callback) {
  mojom::ArcHttpTestResultPtr result_ptr = mojom::ArcHttpTestResult::New();
  result_ptr->is_successful = http_test_result_.is_successful;
  result_ptr->status_code = http_test_result_.status_code;
  result_ptr->duration_ms = http_test_result_.duration_ms;
  std::move(callback).Run(std::move(result_ptr));
}

void FakeNetInstance::PingTest(const std::string& transport_name,
                               const std::string& ip_address,
                               PingTestCallback callback) {
  mojom::ArcPingTestResultPtr result_ptr = mojom::ArcPingTestResult::New();
  result_ptr->is_successful = ping_test_result_.is_successful;
  result_ptr->duration_ms = ping_test_result_.duration_ms;
  std::move(callback).Run(std::move(result_ptr));
}

void FakeNetInstance::SetUpFlag(mojom::Flag flag, bool value) {}

}  // namespace arc
