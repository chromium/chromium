// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_NET_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_NET_INSTANCE_H_

#include "ash/components/arc/mojom/net.mojom.h"

namespace arc {

class FakeNetInstance : public mojom::NetInstance {
 public:
  FakeNetInstance();
  ~FakeNetInstance() override;

  FakeNetInstance(const FakeNetInstance&) = delete;
  FakeNetInstance& operator=(const FakeNetInstance&) = delete;

  void Init(::mojo::PendingRemote<mojom::NetHost> host_remote,
            InitCallback callback) override;

  void ScanCompleted() override;

  void WifiEnabledStateChanged(bool is_enabled) override;

  void DisconnectAndroidVpn() override;

  void ConfigureAndroidVpn() override;

  // TODO(b/308365031): Rename mojo ActiveNetworksChanged to HostNetworksChanged
  void ActiveNetworksChanged(
      std::vector<mojom::NetworkConfigurationPtr> network) override;

  void DnsResolutionTest(const std::string& transport_name,
                         const std::string& host_name,
                         DnsResolutionTestCallback callback) override;

  void HttpTest(const std::string& transport_name,
                const ::GURL& url,
                HttpTestCallback callback) override;

  void PingTest(const std::string& transport_name,
                const std::string& ip_address,
                PingTestCallback callback) override;

  void SetUpFlag(mojom::Flag flag, bool value) override;

  void set_http_test_result(mojom::ArcHttpTestResult http_test_result) {
    http_test_result_ = http_test_result;
  }

  void set_dns_resolution_test_result(
      mojom::ArcDnsResolutionTestResult dns_resolution_test_result) {
    dns_resolution_test_result_ = dns_resolution_test_result;
  }

  void set_ping_test_result(mojom::ArcPingTestResult ping_test_result) {
    ping_test_result_ = ping_test_result;
  }

 private:
  mojom::ArcHttpTestResult http_test_result_;
  mojom::ArcDnsResolutionTestResult dns_resolution_test_result_;
  mojom::ArcPingTestResult ping_test_result_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_NET_INSTANCE_H_
