// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/tcp_device_provider.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/tcp_client_socket.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace {

const char kDeviceModel[] = "Remote Target";
const char kBrowserName[] = "Target";

static void RunSocketCallback(AndroidDeviceManager::SocketCallback callback,
                              std::unique_ptr<net::StreamSocket> socket,
                              int result) {
  std::move(callback).Run(result, std::move(socket));
}

class ResolveHostAndOpenSocket final : public network::ResolveHostClientBase {
 public:
  ResolveHostAndOpenSocket(const net::HostPortPair& address,
                           AndroidDeviceManager::SocketCallback callback)
      : callback_(std::move(callback)) {
    mojo::Remote<network::mojom::HostResolver> resolver;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](mojo::PendingReceiver<network::mojom::HostResolver>
                              pending_receiver) {
                         g_browser_process->system_network_context_manager()
                             ->GetContext()
                             ->CreateHostResolver(std::nullopt,
                                                  std::move(pending_receiver));
                       },
                       resolver.BindNewPipeAndPassReceiver()));
    // Intentionally using a HostPortPair because scheme isn't specified.
    // Fine to use a transient NetworkAnonymizationKey here - this is for
    // debugging, so performance doesn't matter, and it doesn't need to share a
    // DNS cache with anything else.
    resolver->ResolveHost(
        network::mojom::HostResolverHost::NewHostPortPair(address),
        net::NetworkAnonymizationKey::CreateTransient(), nullptr,
        receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(base::BindOnce(
        &ResolveHostAndOpenSocket::OnComplete, base::Unretained(this),
        net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
        /*resolved_addresses=*/std::nullopt,
        /*endpoint_results_with_metadata=*/std::nullopt));
  }

 private:
  // network::mojom::ResolveHostClient implementation:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    if (result != net::OK) {
      RunSocketCallback(std::move(callback_), nullptr,
                        resolve_error_info.error);
      delete this;
      return;
    }
    std::unique_ptr<net::StreamSocket> socket(
        new net::TCPClientSocket(resolved_addresses.value(), nullptr, nullptr,
                                 nullptr, net::NetLogSource()));
    net::StreamSocket* socket_ptr = socket.get();
    auto split_callback = base::SplitOnceCallback(base::BindOnce(
        &RunSocketCallback, std::move(callback_), std::move(socket)));
    result = socket_ptr->Connect(std::move(split_callback.first));
    if (result != net::ERR_IO_PENDING) {
      std::move(split_callback.second).Run(result);
    }
    delete this;
  }

  AndroidDeviceManager::SocketCallback callback_;
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
};

}  // namespace

scoped_refptr<TCPDeviceProvider> TCPDeviceProvider::CreateForLocalhost(
    uint16_t port) {
  TCPDeviceProvider::HostPortSet targets;
  targets.insert(net::HostPortPair("127.0.0.1", port));
  return new TCPDeviceProvider(targets);
}

TCPDeviceProvider::TCPDeviceProvider(const HostPortSet& targets)
    : targets_(targets) {
}

void TCPDeviceProvider::QueryDevices(SerialsCallback callback) {
  std::vector<std::string> result;
  for (const net::HostPortPair& target : targets_) {
    const std::string& host = target.host();
    if (base::Contains(result, host))
      continue;
    result.push_back(host);
  }
  std::move(callback).Run(std::move(result));
}

void TCPDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                        DeviceInfoCallback callback) {
  AndroidDeviceManager::DeviceInfo device_info;
  device_info.model = kDeviceModel;
  device_info.connected = true;

  for (const net::HostPortPair& target : targets_) {
    if (serial != target.host())
      continue;
    AndroidDeviceManager::BrowserInfo browser_info;
    browser_info.socket_name = base::NumberToString(target.port());
    browser_info.display_name = kBrowserName;
    browser_info.type = AndroidDeviceManager::BrowserInfo::kTypeChrome;

    device_info.browser_info.push_back(browser_info);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), device_info));
}

void TCPDeviceProvider::OpenSocket(const std::string& serial,
                                   const std::string& socket_name,
                                   SocketCallback callback) {
  // Use plain socket for remote debugging and port forwarding on Desktop
  // (debugging purposes).
  int port;
  base::StringToInt(socket_name, &port);
  new ResolveHostAndOpenSocket(net::HostPortPair(serial, port),
                               std::move(callback));
}

void TCPDeviceProvider::ReleaseDevice(const std::string& serial) {
  if (!release_callback_.is_null())
    std::move(release_callback_).Run();
}

void TCPDeviceProvider::set_release_callback_for_test(
    base::OnceClosure callback) {
  release_callback_ = std::move(callback);
}

TCPDeviceProvider::~TCPDeviceProvider() {
}
