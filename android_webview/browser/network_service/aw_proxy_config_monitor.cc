// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_proxy_config_monitor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace android_webview {

namespace {

const char kProxyServerSwitch[] = "proxy-server";
const char kProxyBypassListSwitch[] = "proxy-bypass-list";

constexpr net::NetworkTrafficAnnotationTag kProxyConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("webview_proxy_config", R"(
      semantics {
        sender: "Proxy configuration via a command line flag"
        description:
          "Used to fetch HTTP/HTTPS/SOCKS5/PAC proxy configuration when "
          "proxy is configured by the --proxy-server command line flag. "
          "When proxy implies automatic configuration, it can send network "
          "requests in the scope of this annotation."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, and they indicate to use a proxy server."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other: "The proxy server specified in the configuration."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request cannot be disabled in settings. However it will never "
          "be made if user does not run with the '--proxy-server' switch."
        policy_exception_justification:
          "Not implemented, behaviour only available behind a switch."
      })");

}  // namespace

AwProxyConfigMonitor::AwProxyConfigMonitor() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT0("startup", "AwProxyConfigMonitor");
  proxy_config_service_android_ =
      std::make_unique<net::ProxyConfigServiceAndroid>(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
  proxy_config_service_android_->set_exclude_pac_url(true);
  proxy_config_service_android_->AddObserver(this);
}

AwProxyConfigMonitor::~AwProxyConfigMonitor() {
  proxy_config_service_android_->RemoveObserver(this);
}

AwProxyConfigMonitor* AwProxyConfigMonitor::GetInstance() {
  static base::NoDestructor<AwProxyConfigMonitor> instance;
  return instance.get();
}

void AwProxyConfigMonitor::AddProxyToNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kProxyServerSwitch)) {
    std::string proxy = command_line.GetSwitchValueASCII(kProxyServerSwitch);
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(proxy);
    if (command_line.HasSwitch(kProxyBypassListSwitch)) {
      std::string bypass_list =
          command_line.GetSwitchValueASCII(kProxyBypassListSwitch);
      proxy_config.proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }

    network_context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation(proxy_config,
                                       kProxyConfigTrafficAnnotation);
  } else {
    mojo::PendingRemote<network::mojom::ProxyConfigClient> proxy_config_client;
    network_context_params->proxy_config_client_receiver =
        proxy_config_client.InitWithNewPipeAndPassReceiver();
    proxy_config_client_set_.Add(std::move(proxy_config_client));

    net::ProxyConfigWithAnnotation proxy_config;
    net::ProxyConfigService::ConfigAvailability availability =
        proxy_config_service_android_->GetLatestProxyConfig(&proxy_config);
    if (availability == net::ProxyConfigService::CONFIG_VALID)
      network_context_params->initial_proxy_config = proxy_config;
  }
}

void AwProxyConfigMonitor::OnProxyConfigChanged(
    const net::ProxyConfigWithAnnotation& config,
    net::ProxyConfigService::ConfigAvailability availability) {
  for (const auto& proxy_config_client : proxy_config_client_set_) {
    switch (availability) {
      case net::ProxyConfigService::CONFIG_VALID:
        proxy_config_client->OnProxyConfigUpdated(config);
        break;
      case net::ProxyConfigService::CONFIG_UNSET:
        proxy_config_client->OnProxyConfigUpdated(
            net::ProxyConfigWithAnnotation::CreateDirect());
        break;
      case net::ProxyConfigService::CONFIG_PENDING:
        NOTREACHED();
    }
  }
}

std::string AwProxyConfigMonitor::SetProxyOverride(
    const std::vector<net::ProxyConfigServiceAndroid::ProxyOverrideRule>&
        proxy_rules,
    const std::vector<std::string>& bypass_rules,
    const bool reverse_bypass,
    base::OnceClosure callback) {
  return proxy_config_service_android_->SetProxyOverride(
      proxy_rules, bypass_rules, reverse_bypass,
      base::BindOnce(&AwProxyConfigMonitor::FlushProxyConfig,
                     base::Unretained(this), std::move(callback)));
}

void AwProxyConfigMonitor::ClearProxyOverride(base::OnceClosure callback) {
  proxy_config_service_android_->ClearProxyOverride(
      base::BindOnce(&AwProxyConfigMonitor::FlushProxyConfig,
                     base::Unretained(this), std::move(callback)));
}

void AwProxyConfigMonitor::FlushProxyConfig(base::OnceClosure callback) {
  int count = proxy_config_client_set_.size();
  base::RepeatingClosure closure =
      base::BarrierClosure(count, std::move(callback));
  for (auto& proxy_config_client : proxy_config_client_set_)
    proxy_config_client->FlushProxyConfig(closure);
}

}  // namespace android_webview
