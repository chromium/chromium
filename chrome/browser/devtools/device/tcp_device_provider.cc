// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/tcp_device_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/device/adb/adb_client_socket.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/tcp_client_socket.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace {

const char kDeviceModel[] = "Remote Target";
const char kBrowserName[] = "Target";

static void RunSocketCallback(
    const AndroidDeviceManager::SocketCallback& callback,
    std::unique_ptr<net::StreamSocket> socket,
    int result) {
  callback.Run(result, std::move(socket));
}

class ResolveHostAndOpenSocket final : public network::ResolveHostClientBase {
 public:
  ResolveHostAndOpenSocket(const net::HostPortPair& address,
                           const AdbClientSocket::SocketCallback& callback)
      : callback_(callback) {
    mojo::Remote<network::mojom::HostResolver> resolver;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](mojo::PendingReceiver<network::mojom::HostResolver>
                              pending_receiver) {
                         g_browser_process->system_network_context_manager()
                             ->GetContext()
                             ->CreateHostResolver(base::nullopt,
                                                  std::move(pending_receiver));
                       },
                       resolver.BindNewPipeAndPassReceiver()));
    // Fine to use a transient NetworkIsolationKey here - this is for debugging,
    // so performance doesn't matter, and it doesn't need to share a DNS cache
    // with anything else.
    resolver->ResolveHost(address, net::NetworkIsolationKey::CreateTransient(),
                          nullptr, receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(
        base::BindOnce(&ResolveHostAndOpenSocket::OnComplete,
                       base::Unretained(this), net::ERR_NAME_NOT_RESOLVED,
                       net::ResolveErrorInfo(net::ERR_FAILED), base::nullopt));
  }

 private:
  // network::mojom::ResolveHostClient implementation:
  void OnComplete(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<net::AddressList>& resolved_addresses) override {
    if (result != net::OK) {
      RunSocketCallback(callback_, nullptr, resolve_error_info.error);
      delete this;
      return;
    }
    std::unique_ptr<net::StreamSocket> socket(
        new net::TCPClientSocket(resolved_addresses.value(), nullptr, nullptr,
                                 nullptr, net::NetLogSource()));
    net::StreamSocket* socket_ptr = socket.get();
    net::CompletionRepeatingCallback on_connect =
        base::AdaptCallbackForRepeating(
            base::BindOnce(&RunSocketCallback, callback_, std::move(socket)));
    result = socket_ptr->Connect(on_connect);
    if (result != net::ERR_IO_PENDING)
      on_connect.Run(result);
    delete this;
  }

  AdbClientSocket::SocketCallback callback_;
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

void TCPDeviceProvider::QueryDevices(const SerialsCallback& callback) {
  std::vector<std::string> result;
  for (const net::HostPortPair& target : targets_) {
    const std::string& host = target.host();
    if (base::Contains(result, host))
      continue;
    result.push_back(host);
  }
  callback.Run(result);
}

void TCPDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                        const DeviceInfoCallback& callback) {
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

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, device_info));
}

void TCPDeviceProvider::OpenSocket(const std::string& serial,
                                   const std::string& socket_name,
                                   const SocketCallback& callback) {
  // Use plain socket for remote debugging and port forwarding on Desktop
  // (debugging purposes).
  int port;
  base::StringToInt(socket_name, &port);
  new ResolveHostAndOpenSocket(net::HostPortPair(serial, port), callback);
}

void TCPDeviceProvider::ReleaseDevice(const std::string& serial) {
  if (!release_callback_.is_null())
    release_callback_.Run();
}

void TCPDeviceProvider::set_release_callback_for_test(
    const base::Closure& callback) {
  release_callback_ = callback;
}

TCPDeviceProvider::~TCPDeviceProvider() {
}
