// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_PROXY_CONFIG_MONITOR_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_PROXY_CONFIG_MONITOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace android_webview {

// This class configures proxy settings for NetworkContext if network service
// is enabled.
class AwProxyConfigMonitor : public net::ProxyConfigService::Observer {
 public:
  AwProxyConfigMonitor(const AwProxyConfigMonitor&) = delete;
  AwProxyConfigMonitor& operator=(const AwProxyConfigMonitor&) = delete;

  static AwProxyConfigMonitor* GetInstance();

  void AddProxyToNetworkContextParams(
      network::mojom::NetworkContextParamsPtr& network_context_params);
  std::string SetProxyOverride(
      const std::vector<net::ProxyConfigServiceAndroid::ProxyOverrideRule>&
          proxy_rules,
      const std::vector<std::string>& bypass_rules,
      base::OnceClosure callback);
  void ClearProxyOverride(base::OnceClosure callback);

 private:
  AwProxyConfigMonitor();
  ~AwProxyConfigMonitor() override;

  friend class base::NoDestructor<AwProxyConfigMonitor>;
  // net::ProxyConfigService::Observer implementation:
  void OnProxyConfigChanged(
      const net::ProxyConfigWithAnnotation& config,
      net::ProxyConfigService::ConfigAvailability availability) override;

  void FlushProxyConfig(base::OnceClosure callback);

  std::unique_ptr<net::ProxyConfigServiceAndroid> proxy_config_service_android_;
  mojo::RemoteSet<network::mojom::ProxyConfigClient> proxy_config_client_set_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_PROXY_CONFIG_MONITOR_H_
